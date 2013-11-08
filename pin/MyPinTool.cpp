#include <stdio.h>
#include <time.h>
#include <list>
#include <map>
#include <assert.h>
#include "pin.H"

#include <iostream>
#include <iterator>

#define CONVERT(type, data_ptr) ((type)((void*)data_ptr))
#define NO_ID ((UINT32) 0xFFFFFFFF)
#define MAX_VC_SIZE 32

// set 1 GB limit to mutex pointer
#define MUTEX_POINTER_LIMIT 0x40000000

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
		"o", "lock_mt.out", "specify output file name");

typedef vector<UINT32> VectorClock;

typedef std::list<UINT32> WaitQueue;
typedef std::map< long, WaitQueue* > WaitQueueMap;
typedef WaitQueueMap::iterator WaitQueueIterator;

class SignalThreadInfo 
{
	public:
		UINT32 threadId;
		VectorClock vectorClock;

		SignalThreadInfo() : threadId(NO_ID) {}

		SignalThreadInfo (UINT32 threadId, const VectorClock& vc) :
			threadId(threadId), vectorClock(vc) {}

		SignalThreadInfo& operator= (const SignalThreadInfo& other)
		{
			SignalThreadInfo temp (other);
			threadId = other.threadId;
			vectorClock = other.vectorClock;

			return *this;
		}

		void update(UINT32 tid, const VectorClock& vc)
		{
			threadId    = tid;
			vectorClock = vc;
		}
};
typedef std::map< long, SignalThreadInfo > SignalThreadMap;
typedef SignalThreadMap::iterator SignalThreadIterator;

// thread local storage
TLS_KEY tlsKey;
TLS_KEY mutexPtrKey;
TLS_KEY vectorClockKey;

UINT32 globalId=0;
PIN_LOCK lock;

WaitQueueMap* waitQueueMap;
SignalThreadMap* signalledThreadMap;

// This routine is executed every time a thread is created.
VOID ThreadStart(THREADID threadId, CONTEXT *ctxt, INT32 flags, VOID *v)
{
	GetLock(&lock, threadId+1);
	++globalId;
	ReleaseLock(&lock);

	string filename = KnobOutputFile.Value() +"." + decstr(threadId);
	FILE* out       = fopen(filename.c_str(), "w");
	fprintf(out, "thread begins... PIN_TID:%d OS_TID:0x%x\n",threadId,PIN_GetTid());
	fflush(out);
	PIN_SetThreadData(tlsKey, out, threadId);
	PIN_SetThreadData(mutexPtrKey, 0, threadId);

	assert(threadId < MAX_VC_SIZE);
	VectorClock* vectorClock = new vector<UINT32>(MAX_VC_SIZE, 0);
	PIN_SetThreadData(vectorClockKey, vectorClock, threadId);
}

// This routine is executed every time a thread is destroyed.
VOID ThreadFini(THREADID threadId, const CONTEXT *ctxt, INT32 code, VOID *v)
{
	FILE* out   = static_cast<FILE*>(PIN_GetThreadData(tlsKey, threadId));
	fclose(out);
	PIN_SetThreadData(tlsKey, 0, threadId);
	PIN_SetThreadData(mutexPtrKey, 0, threadId);
}

// This routine is executed each time lock is called.
VOID BeforeLock (pthread_mutex_t * mutex, THREADID threadId)
{
	if(CONVERT(long, mutex) > MUTEX_POINTER_LIMIT)
	{
		PIN_SetThreadData(mutexPtrKey, mutex, threadId);
		return;
	}

	WaitQueue* mutexWaitList = NULL;
	// point to the current mutex
	PIN_SetThreadData(mutexPtrKey, mutex, threadId);

	GetLock(&lock, threadId+1);

	WaitQueueIterator foundQueueItr = waitQueueMap->find(CONVERT(long, mutex));
	if(foundQueueItr != waitQueueMap->end())
	{
		mutexWaitList = foundQueueItr->second;
	}

	if (mutexWaitList) 
	{
		mutexWaitList->push_back(threadId);
	}
	else
	{
		mutexWaitList = new std::list<UINT32>;
		mutexWaitList->push_back(threadId);
		(*waitQueueMap)[CONVERT(long, mutex)] = mutexWaitList;
	}

	ReleaseLock(&lock);
}

static void updateVectorClock(UINT32 myThreadId, VectorClock& myClock, VectorClock& otherClock)
{
	for (unsigned int i = 0; i < MAX_VC_SIZE ; i++) 
	{
		if (i == myThreadId) 
		{
			continue;
		}

		myClock[i] = std::max(myClock[i], otherClock[i]);
	}
}

VOID AfterLock (THREADID threadId)
{
	WaitQueue* mutexWaitList = NULL;
	pthread_mutex_t* mutex = static_cast<pthread_mutex_t*>(PIN_GetThreadData(mutexPtrKey, threadId));
	if(CONVERT(long, mutex) > MUTEX_POINTER_LIMIT)
		return;

	printf("Thread %d acquired a lock[%p].\n", threadId, mutex);
	VectorClock* vectorClock = static_cast<VectorClock*>(PIN_GetThreadData(vectorClockKey, threadId));
	(*vectorClock)[threadId]++;

	// get the signalling thread
	GetLock(&lock, threadId+1);
	SignalThreadIterator itr = signalledThreadMap->find(CONVERT(long, mutex));


	if(itr != signalledThreadMap->end())
	{
		SignalThreadInfo signalThreadInfo = itr->second;
		// update own clock

		if (signalThreadInfo.threadId != NO_ID) 
		{
			updateVectorClock(threadId, *vectorClock, signalThreadInfo.vectorClock);
			printf("Thread %d happens before %d due to lock %p\n", signalThreadInfo.threadId, threadId, mutex);
		}


		// remove the notify signal
		SignalThreadIterator stiItr = signalledThreadMap->find(CONVERT(long, mutex));
		stiItr->second.threadId = NO_ID;
	}

	printf("New vector count :\n");
	for (VectorClock::iterator vci = vectorClock->begin(); vci != vectorClock->end() ; vci++) 
	{
		printf("%d, ", *vci);
	}
	printf("\n");

	WaitQueueIterator foundQueueItr = waitQueueMap->find(CONVERT(long, mutex));
	if(foundQueueItr != waitQueueMap->end() && mutexWaitList)
	{
		mutexWaitList = foundQueueItr->second;
		mutexWaitList->remove(threadId);
	}

	ReleaseLock(&lock);
}

VOID BeforeUnlock (pthread_mutex_t * mutex, THREADID threadId)
{
	if(CONVERT(long, mutex) > MUTEX_POINTER_LIMIT)
		return;

	WaitQueue* mutexWaitList = NULL;
	VectorClock* vectorClock = static_cast<VectorClock*>(PIN_GetThreadData(vectorClockKey, threadId));

	GetLock(&lock, threadId+1);
	WaitQueueIterator foundQueueItr = waitQueueMap->find(CONVERT(long, mutex));
	if(foundQueueItr != waitQueueMap->end())
	{
		mutexWaitList = foundQueueItr->second;
	}

	// In new epoch
	(*vectorClock)[threadId]++;

	if (mutexWaitList && mutexWaitList->size()) 
	{
		(*signalledThreadMap)[CONVERT(long, mutex)].update(threadId, *vectorClock);
	}
	else 
	{
		// lost notify
		(*signalledThreadMap)[CONVERT(long, mutex)].threadId = NO_ID;
	}
	printf("Thread %d released a lock[%p]. New VC: \n", threadId, mutex);
	for (VectorClock::iterator vci = vectorClock->begin(); vci != vectorClock->end() ; vci++) 
	{
		printf("%d, ", *vci);
	}
	printf("\n");
	ReleaseLock(&lock);

}

// This routine is executed for each image.
VOID ImageLoad (IMG img, VOID *)
{
	RTN rtn = RTN_FindByName(img, "pthread_mutex_lock");
	if (RTN_Valid(rtn))
	{
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(BeforeLock),
				IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
				IARG_THREAD_ID, IARG_END);
		RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(AfterLock),
				IARG_THREAD_ID, IARG_END);
		RTN_Close(rtn);
	}

	rtn = RTN_FindByName(img, "pthread_mutex_unlock");
	if (RTN_Valid(rtn))
	{
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(BeforeUnlock),
				IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
				IARG_THREAD_ID, IARG_END);
		RTN_Close(rtn);
	}
}


INT32 Usage ()
{
	PIN_ERROR("This Pintool prints a trace of pthread_mutex_lock and pthread_mutex_unlock  calls in the guest application\n"
			+ KNOB_BASE::StringKnobSummary() + "\n");
	return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main (INT32 argc, CHAR **argv)
{
	InitLock(&lock);

	waitQueueMap = new WaitQueueMap;
	signalledThreadMap = new SignalThreadMap;

	// Initialize pin
	if (PIN_Init(argc, argv)) return Usage();
	PIN_InitSymbols();

	tlsKey         = PIN_CreateThreadDataKey(0);
	mutexPtrKey    = PIN_CreateThreadDataKey(0);
	vectorClockKey = PIN_CreateThreadDataKey(0);

	// Register ImageLoad to be called when each image is loaded.
	IMG_AddInstrumentFunction(ImageLoad, 0);

	// Register Analysis routines to be called when a thread begins/ends
	PIN_AddThreadStartFunction(ThreadStart, 0);
	PIN_AddThreadFiniFunction(ThreadFini, 0);

	// Never returns
	PIN_StartProgram();

	return 0;
}
