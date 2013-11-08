#include <stdio.h>
#include <time.h>
#include <list>
#include <map>
#include "pin.H"

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
		"o", "lock_mt.out", "specify output file name");

typedef std::map<long int, std::list<unsigned int> >::iterator WaitQueueIterator;
#define CONVERT(type, data_ptr) ((type)((void*)data_ptr))

// thread local storage
TLS_KEY tlsKey;
TLS_KEY idKey;
TLS_KEY mutexPtrKey;

UINT32 globalId=0;
PIN_LOCK lock;

PIN_LOCK waitQueueLock;
std::map< long, std::list<UINT32>* >* waitQueue;

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
	//FILE* out = stderr;
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
	FILE* out  = static_cast<FILE*>(PIN_GetThreadData(tlsKey, threadid));
	UINT32* id = static_cast<UINT32*>(PIN_GetThreadData(idKey, threadid));
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	fprintf(out, "thread(%u)::pthread_mutex_lock-bf(%p)::time(%ld,%ld)\n",(*id),mutex,ts.tv_sec,ts.tv_nsec);

	// point to the current mutex
	PIN_SetThreadData(mutexPtrKey, mutex, threadid);

	GetLock(&waitQueueLock, threadid+1);

	WaitQueueIterator it = waitQueue->find(*CONVERT(long*, mutex));
	if(it != waitQueue->end())
		mutexWaitList = it->second;

	if (mutexWaitList) 
	{
		fprintf(out,"In the wait list of %p : \n", mutex);
		for (std::list<UINT32>::iterator it = mutexWaitList->begin(); it != mutexWaitList->end(); it++)
		{
			fprintf(out,"%d, ", *it);
		}
		fprintf(out,"\n");

		mutexWaitList->push_back(*id);
	}
	else
	{
		mutexWaitList = new std::list<UINT32>;
		mutexWaitList->push_back(*id);
		//fprintf(out,"In before lock -- %p\n", mutexWaitList);
		waitQueue[mutex] = mutexWaitList;
	}
	ReleaseLock(&waitQueueLock);
}

VOID AfterLock (THREADID threadid)
{
	std::list<UINT32>* mutexWaitList = NULL;
	FILE* out  = static_cast<FILE*>(PIN_GetThreadData(tlsKey, threadid));
	UINT32* id = static_cast<UINT32*>(PIN_GetThreadData(idKey, threadid));
	pthread_mutex_t* mutex = static_cast<pthread_mutex_t*>(PIN_GetThreadData(mutexPtrKey, threadid));
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	fprintf(out, "thread(%u)::pthread_mutex_lock-af(%p)::time(%ld,%ld)\n",(*id),mutex,ts.tv_sec,ts.tv_nsec);

	GetLock(&waitQueueLock, threadid+1);
	fprintf(out,"after lock in of mutex:%p\n", mutex);

	auto it = waitQueue->find(*CONVERT(long*, mutex));
	if(it != waitQueue->end())
		mutexWaitList = it->second;

	fprintf(out,"waiting list:%p\n", mutexWaitList);
	//mutexWaitList->remove(*id);
	ReleaseLock(&waitQueueLock);
}

VOID BeforeUnlock (pthread_mutex_t * mutex, THREADID threadid)
{
	//fprintf(out,"In before unlock\n");
	FILE* out  = static_cast<FILE*>(PIN_GetThreadData(tlsKey, threadid));
	UINT32* id = static_cast<UINT32*>(PIN_GetThreadData(idKey, threadid));
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	if ((void*) mutex != (void*) &lock) 
	{
		fprintf(out, "thread(%u)::pthread_mutex_unlock(%p)::time(%ld,%ld)\n", (*id),mutex,ts.tv_sec,ts.tv_nsec);
	}
	else 
	{
		fprintf(stderr, "In our lock\n");
	}
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
	InitLock(&waitQueueLock);
	waitQueue = new std::map< long, std::list<UINT32>* >;
	printf("wait queue: %p\n", waitQueue);

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
