#include <stdio.h>
#include <time.h>
#include <list>
#include <map>
#include <assert.h>
#include "pin.H"

#define CONVERT(type, data_ptr) ((type)((void*)data_ptr))
#define NO_ID ((UINT32) 0xFFFFFFFF)
#define MAX_VC_SIZE 32

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
		"o", "lock_mt.out", "specify output file name");

typedef std::list<UINT32> WaitQueue;
typedef std::map< long, WaitQueue* > WaitQueueMap;
typedef WaitQueueMap::iterator WaitQueueIterator;

struct STI 
{
	UINT32 threadId;
	vector<UINT32>* vectorClock;

	STI() : threadId(NO_ID), vectorClock(NULL) {}
	STI(UINT32 threadId, vector<UINT32>* vc) :
		threadId(threadId), vectorClock(vc) {}
};
typedef struct STI SignalThreadInfo;
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

	vector<UINT32>* vectorClock = new vector<UINT32>;
	vectorClock->resize(MAX_VC_SIZE, 0);
	assert(threadId < MAX_VC_SIZE);
	(*vectorClock)[threadId] = 1;
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

VOID AfterLock (THREADID threadId)
{
	WaitQueue* mutexWaitList = NULL;
	pthread_mutex_t* mutex = static_cast<pthread_mutex_t*>(PIN_GetThreadData(mutexPtrKey, threadId));
	//vector<UINT32>* vectorClock = static_cast<vector<UINT32>*>(PIN_GetThreadData(vectorClockKey, threadId));

	//timespec ts;
	//clock_gettime(CLOCK_REALTIME, &ts);
	//fprintf(out, "thread(%u)::pthread_mutex_lock-af(%p)::time(%ld,%ld)\n",(*id),mutex,ts.tv_sec,ts.tv_nsec);

	// get the signalling thread
	GetLock(&lock, threadId+1);
	SignalThreadIterator itr = signalledThreadMap->find(CONVERT(long, mutex));
	if(itr != signalledThreadMap->end())
	{
		SignalThreadInfo signalThreadInfo = itr->second;
		if (signalThreadInfo.threadId != NO_ID) 
		{
			printf("Thread %d happens before %d due to lock %p\n", signalThreadInfo.threadId, threadId, mutex);
		}
		// remove the notify signal
		(*signalledThreadMap)[CONVERT(long, mutex)] = SignalThreadInfo();
	}

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
	WaitQueue* mutexWaitList = NULL;

	GetLock(&lock, threadId+1);
	WaitQueueIterator foundQueueItr = waitQueueMap->find(CONVERT(long, mutex));
	if(foundQueueItr != waitQueueMap->end())
	{
		mutexWaitList = foundQueueItr->second;
	}

	if (mutexWaitList && mutexWaitList->size()) 
	{
		(*signalledThreadMap)[CONVERT(long, mutex)] = SignalThreadInfo(threadId, NULL);
	}
	else 
	{
		// lost notify
		(*signalledThreadMap)[CONVERT(long, mutex)] = SignalThreadInfo();
	}
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
