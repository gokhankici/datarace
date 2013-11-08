#include <stdio.h>
#include <time.h>
#include <list>
#include <map>
#include "pin.H"

#define CONVERT(type, data_ptr) ((type)((void*)data_ptr))
#define NO_ID ((UINT32) 0xFFFFFFFF)
#define MAX_VC_SIZE 32

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
		"o", "lock_mt.out", "specify output file name");

typedef std::map< long, std::list<UINT32>* >::iterator WaitQueueIterator;
typedef std::map< long, UINT32 >::iterator SignalledThreadIterator;

// thread local storage
TLS_KEY tlsKey;
TLS_KEY idKey;
TLS_KEY mutexPtrKey;

UINT32 globalId=0;
PIN_LOCK lock;

std::map< long, std::list<UINT32>* >* waitQueue;
std::map< long, UINT32 >* signalledThread;

// This routine is executed every time a thread is created.
VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
	UINT32* id = new UINT32;
	GetLock(&lock, threadid+1);
	++globalId;
	(*id)=globalId;
	PIN_SetThreadData(idKey, id, threadid);
	ReleaseLock(&lock);

	string filename = KnobOutputFile.Value() +"." + decstr(*id);
	FILE* out       = fopen(filename.c_str(), "w");
	fprintf(out, "thread begins... MYID:%u PIN_TID:%d OS_TID:0x%x\n",(*id),threadid,PIN_GetTid());
	fflush(out);
	PIN_SetThreadData(tlsKey, out, threadid);
	PIN_SetThreadData(mutexPtrKey, 0, threadid);
}

// This routine is executed every time a thread is destroyed.
VOID ThreadFini(THREADID threadid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
	FILE* out   = static_cast<FILE*>(PIN_GetThreadData(tlsKey, threadid));
	UINT32* id = static_cast<UINT32*>(PIN_GetThreadData(idKey, threadid));
	fclose(out);
	delete id;
	PIN_SetThreadData(tlsKey, 0, threadid);
	PIN_SetThreadData(idKey, 0, threadid);
	PIN_SetThreadData(mutexPtrKey, 0, threadid);
}

// This routine is executed each time lock is called.
VOID BeforeLock (pthread_mutex_t * mutex, THREADID threadid)
{
	std::list<UINT32>* mutexWaitList = NULL;
	UINT32* id = static_cast<UINT32*>(PIN_GetThreadData(idKey, threadid));

	// point to the current mutex
	PIN_SetThreadData(mutexPtrKey, mutex, threadid);

	GetLock(&lock, threadid+1);

	WaitQueueIterator foundQueueItr = waitQueue->find(CONVERT(long, mutex));
	if(foundQueueItr != waitQueue->end())
	{
		mutexWaitList = foundQueueItr->second;
	}

	if (mutexWaitList) 
	{
		mutexWaitList->push_back(*id);
	}
	else
	{
		mutexWaitList = new std::list<UINT32>;
		mutexWaitList->push_back(*id);
		(*waitQueue)[CONVERT(long, mutex)] = mutexWaitList;
	}

	ReleaseLock(&lock);
}

VOID AfterLock (THREADID threadid)
{
	std::list<UINT32>* mutexWaitList = NULL;
	UINT32* id = static_cast<UINT32*>(PIN_GetThreadData(idKey, threadid));
	pthread_mutex_t* mutex = static_cast<pthread_mutex_t*>(PIN_GetThreadData(mutexPtrKey, threadid));
	//timespec ts;
	//clock_gettime(CLOCK_REALTIME, &ts);
	//fprintf(out, "thread(%u)::pthread_mutex_lock-af(%p)::time(%ld,%ld)\n",(*id),mutex,ts.tv_sec,ts.tv_nsec);

	// get the signalling thread
	GetLock(&lock, threadid+1);
	SignalledThreadIterator itr = signalledThread->find(CONVERT(long, mutex));
	if(itr != signalledThread->end())
	{
		UINT32 sigThread = itr->second;
		if (sigThread != NO_ID) 
		{
			printf("Thread %d happens before %d due to lock %p\n", sigThread, *id, mutex);
		}
		// remove the notify signal
		(*signalledThread)[CONVERT(long, mutex)] = NO_ID;
	}

	WaitQueueIterator foundQueueItr = waitQueue->find(CONVERT(long, mutex));
	if(foundQueueItr != waitQueue->end() && mutexWaitList)
	{
		mutexWaitList = foundQueueItr->second;
		mutexWaitList->remove(*id);
	}
	ReleaseLock(&lock);
}

VOID BeforeUnlock (pthread_mutex_t * mutex, THREADID threadid)
{
	std::list<UINT32>* mutexWaitList = NULL;
	UINT32* id = static_cast<UINT32*>(PIN_GetThreadData(idKey, threadid));

	GetLock(&lock, threadid+1);
	WaitQueueIterator foundQueueItr = waitQueue->find(CONVERT(long, mutex));
	if(foundQueueItr != waitQueue->end())
	{
		mutexWaitList = foundQueueItr->second;
	}

	if (mutexWaitList && mutexWaitList->size()) 
	{
		(*signalledThread)[CONVERT(long, mutex)] = *id;
	}
	else 
	{
		// lost notify
		(*signalledThread)[CONVERT(long, mutex)] = NO_ID;
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

	waitQueue = new std::map< long, std::list<UINT32>* >;
	signalledThread = new std::map<long, UINT32>;

	// Initialize pin
	if (PIN_Init(argc, argv)) return Usage();
	PIN_InitSymbols();

	tlsKey = PIN_CreateThreadDataKey(0);
	idKey  = PIN_CreateThreadDataKey(0);
	mutexPtrKey = PIN_CreateThreadDataKey(0);

	// Register ImageLoad to be called when each image is loaded.
	IMG_AddInstrumentFunction(ImageLoad, 0);

	// Register Analysis routines to be called when a thread begins/ends
	PIN_AddThreadStartFunction(ThreadStart, 0);
	PIN_AddThreadFiniFunction(ThreadFini, 0);

	// Never returns
	PIN_StartProgram();

	return 0;
}
