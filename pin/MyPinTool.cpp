#include <stdio.h>
#include <time.h>
#include <list>
#include "pin.H"
#include "adt/hashmap.h"
#include "YOOP/LinkedList.h"

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
		"o", "lock_mt.out", "specify output file name");

#define CONVERT(type, data_ptr) ((type)((void*)data_ptr))

// thread local storage
TLS_KEY tls_key;
TLS_KEY id_key;

UINT32 global_id=0;
PIN_LOCK lock;

PIN_LOCK wait_queue_lock;
hashmap* wait_queue;

// This routine is executed every time a thread is created.
VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
	UINT32* id = new UINT32;
	GetLock(&lock, threadid+1);
	++global_id;
	(*id)=global_id;
	PIN_SetThreadData(id_key, id, threadid);
	ReleaseLock(&lock);

	string filename = KnobOutputFile.Value() +"." + decstr(*id);
	FILE* out       = fopen(filename.c_str(), "w");
	//FILE* out = stderr;
	fprintf(out, "thread begins... MYID:%u PIN_TID:%d OS_TID:0x%x\n",(*id),threadid,PIN_GetTid());
	fflush(out);
	PIN_SetThreadData(tls_key, out, threadid);
}

// This routine is executed every time a thread is destroyed.
VOID ThreadFini(THREADID threadid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
	FILE* out   = static_cast<FILE*>(PIN_GetThreadData(tls_key, threadid));
	UINT32* id = static_cast<UINT32*>(PIN_GetThreadData(id_key, threadid));
	fclose(out);
	delete id;
	PIN_SetThreadData(tls_key, 0, threadid);
	PIN_SetThreadData(id_key, 0, threadid);
}

// This routine is executed each time lock is called.
VOID BeforeLock (pthread_mutex_t * mutex, THREADID threadid)
{
	LinkedList* mutexWaitList = NULL;
	FILE* out  = static_cast<FILE*>(PIN_GetThreadData(tls_key, threadid));
	UINT32* id = static_cast<UINT32*>(PIN_GetThreadData(id_key, threadid));
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	fprintf(out, "thread(%u)::pthread_mutex_lock-bf(%p)::time(%ld,%ld)\n",(*id),mutex,ts.tv_sec,ts.tv_nsec);

	GetLock(&wait_queue_lock, threadid+1);
	mutexWaitList = (LinkedList*) hashmapGet(wait_queue, *CONVERT(long*, mutex));
	if (mutexWaitList) 
	{
		UnaryNode* node;
		fprintf(out, "Waiting on %p : ", (void*) mutex);
		for (node = mutexWaitList->begin; node; node = node->next) 
		{
			fprintf(out, "%d, ", *((UINT32*)node->object));
		}
		fprintf(out, "\n");

		mutexWaitList->add(id, defaultDeleteUnaryNode);
	}
	else
	{
		mutexWaitList = createLinkedList();
		mutexWaitList->add(id, defaultDeleteUnaryNode);
		hashmapInsert(wait_queue, mutexWaitList, *id);
	}
	ReleaseLock(&wait_queue_lock);
}

// This routine is executed each time lock is called.
VOID AfterLock (pthread_mutex_t * mutex, THREADID threadid)
{
	FILE* out  = static_cast<FILE*>(PIN_GetThreadData(tls_key, threadid));
	UINT32* id = static_cast<UINT32*>(PIN_GetThreadData(id_key, threadid));
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	fprintf(out, "thread(%u)::pthread_mutex_lock-af(%p)::time(%ld,%ld)\n",(*id),mutex,ts.tv_sec,ts.tv_nsec);
}

VOID BeforeUnlock (pthread_mutex_t * mutex, THREADID threadid)
{
	FILE* out  = static_cast<FILE*>(PIN_GetThreadData(tls_key, threadid));
	UINT32* id = static_cast<UINT32*>(PIN_GetThreadData(id_key, threadid));
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
				IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
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
	printf("Image loaded\n");
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
	InitLock(&wait_queue_lock);
	wait_queue = hashmapCreate(0);
	printf("wait queue: %p\n", wait_queue);

	// Initialize pin
	if (PIN_Init(argc, argv)) return Usage();
	PIN_InitSymbols();

	tls_key = PIN_CreateThreadDataKey(0);
	id_key  = PIN_CreateThreadDataKey(0);

	// Register ImageLoad to be called when each image is loaded.
	IMG_AddInstrumentFunction(ImageLoad, 0);

	// Register Analysis routines to be called when a thread begins/ends
	PIN_AddThreadStartFunction(ThreadStart, 0);
	PIN_AddThreadFiniFunction(ThreadFini, 0);

	// Never returns
	PIN_StartProgram();

	return 0;
}
