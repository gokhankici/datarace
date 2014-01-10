#include <stdio.h>
#include <assert.h>
#include <algorithm>

#include "PthreadInstumentation.h"
#include "GlobalVariables.h"
#include "RecordNReplay.h"

// define the replaced function here for convenience
enum
{
    __PTHREAD_CREATE__
};


/*static void writeInHex(const unsigned char* data, int len)
 {
 for (int i = 0; i < len; i++)
 {
 printf("%02X", data[i]);
 }
 }*/

/*
 * Get the local storage of the thread with the given id
 */
inline static
ThreadLocalStorage* getTLS(THREADID tid)
{
	ThreadLocalStorage* tls = static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	assert(tls || (printf("Failed for thread %d\n", tid) && false));

	return tls;
}

/*
 * Log the timestamp and signatures
 */
static void printSignatures(THREADID tid)
{
	ThreadLocalStorage* tls = getTLS(tid);
	assert(tls && tls->out && tls->vectorClock);

	FILE* out = tls->out;
	VectorClock* vectorClock = tls->vectorClock;

#ifdef DEBUG_MODE

	cout << *vectorClock << "read: " << *tls->readBloomFilter << "write: " << *tls->writeBloomFilter << endl;
	fflush(stdout);
#endif

	vectorClock->printVector(out);
	fflush(out);
}

static void printSignatures()
{
	return printSignatures(PIN_ThreadId());
}

/*
 * Initialize the thread local storage and create the vector clock of the thread
 */
VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
	assert(tid < MAX_THREAD_COUNT);

	GetLock(&threadIdMapLock, tid + 1);

	PrintRecordInfo(tid, CREATE);

	rdm.addProcessor();

	ThreadLocalStorage* tls = new ThreadLocalStorage();
	tls->readBloomFilter = new Bloom();
	tls->writeBloomFilter = new Bloom();

	string filename = KnobOutputFile.Value() + "." + decstr(tid);
	FILE* out = fopen(filename.c_str(), "w");
	tls->out = out;

	// put my OS-ThreadId / PIN-ThreadId mapping
	threadIdMap[PIN_GetTid()] = tid;

	// zero means the top of the process tree
	INT32 parentOS_TID = PIN_GetParentTid();
	if (parentOS_TID)
	{
		// find parent's thread id
		ThreadIdMapItr parentTidItr = threadIdMap.find(parentOS_TID);
		assert(parentTidItr != threadIdMap.end());
		THREADID parentTID = parentTidItr->second;

		CreateInfo thisThreadInfo(tid, parentTID);
		threadCreateOrder.push_back(thisThreadInfo);

		ThreadLocalStorage* parentTls = getTLS(parentTID);

		// calculate vector clock
		ChildVCMapItr itr = parentTls->createVCMap.find(tid);
		// TODO: fix here
		int count = 0;
		do
		{
			ReleaseLock(&threadIdMapLock);
			PIN_Sleep(50);
			count++;
			GetLock(&threadIdMapLock, tid + 1);
		}
		while(itr == parentTls->createVCMap.end() && count < 100);

		assert(count != 100 ||
		       (printf("TID: %d ### Parent TID: %d\n", tid, parentTID) && 0));

		tls->vectorClock = new VectorClock(itr->second, tid);
		parentTls->createVCMap.erase(itr);
	}
	else
	{
		tls->vectorClock = new VectorClock(tid);
	}

	ReleaseLock(&threadIdMapLock);

	// create the log file
	PIN_SetThreadData(tlsKey, tls, tid);
}

// This routine is executed every time a thread is destroyed.
VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
	if (recordFile)
		fflush(recordFile);

	ThreadLocalStorage* tls = getTLS(tid);
	FILE* out = tls->out;
	VectorClock* vectorClock = tls->vectorClock;
	Bloom* readFilter = tls->readBloomFilter;
	Bloom* writeFilter = tls->writeBloomFilter;

	GetLock(&rdmLock, tid + 1);

	printSignatures();
	rdm.addSignature(new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));

	ReleaseLock(&rdmLock);

	/* update parent thread's vector clock with the finished child's */
	GetLock(&threadIdMapLock, tid + 1);

	// zero means the top of the process tree
	INT32 parentOS_TID = PIN_GetParentTid();
	if (parentOS_TID)
	{
		// find parent's thread id
		ThreadIdMapItr parentTidItr = threadIdMap.find(parentOS_TID);
		assert(parentTidItr != threadIdMap.end());
		THREADID parentTID = parentTidItr->second;

		// get its vector clock
		ThreadLocalStorage* parentTLS = getTLS(parentTID);
		parentTLS->joinVCMap[tid] = *tls->vectorClock;
	}

	ReleaseLock(&threadIdMapLock);

	fclose(out);
	delete tls;

	PIN_SetThreadData(tlsKey, 0, tid);
	printf("thread %d tls'ini sildi\n", tid); // HERE
}

VOID PthreadCreateReplacement(THREADID tid, CONTEXT *ctxt, AFUNPTR origFunc,
                              pthread_t *__restrict newthread,
                              __const pthread_attr_t *__restrict attr,
                              void *(*start_routine)(void *),
                              void *__restrict arg)
{
	GetLock(&atomicCreate, tid + 1);

	ThreadLocalStorage* tls = getTLS(tid);

	GetLock(&threadIdMapLock, tid + 1);
	tls->createVCMap[globalCreatedThreadId] = *tls->vectorClock;
	printf("Thread %d is putting %d's vc into create map\n", tid, globalCreatedThreadId); // HERE
	ReleaseLock(&threadIdMapLock);

	GetLock(&rdmLock, tid + 1);

	printSignatures();

#ifdef PRINT_SYNC_FUNCTION

	fprintf(tls->out, "--- PTHREAD CREATE ---\n");
#endif

	// add current signature to the rdm
	rdm.addSignature( new SigRaceData(tid, *tls->vectorClock,
	                                  *tls->readBloomFilter, *tls->writeBloomFilter));
	ReleaseLock(&rdmLock);

	tls->readBloomFilter->clear();
	tls->writeBloomFilter->clear();
	tls->vectorClock->advance();

	//	MyStartRoutineArgs* myArgs = new MyStartRoutineArgs(arg, newthread);

	int rc = 0;
	PIN_CallApplicationFunction(ctxt, tid, CALLINGSTD_DEFAULT, origFunc,
	                            PIN_PARG(int), &rc,
	                            PIN_PARG(pthread_t*), newthread,
	                            PIN_PARG(__const pthread_attr_t *), attr,
	                            PIN_PARG(void *(*)(void *)), start_routine,
	                            PIN_PARG(void*), arg,
	                            PIN_PARG_END());

	GetLock(&threadIdMapLock, tid + 1);
	pthreadPinIdMap[*newthread] = globalCreatedThreadId;
	globalCreatedThreadId++;
	ReleaseLock(&threadIdMapLock);

	ReleaseLock(&atomicCreate);
}

VOID BeforeJoin(THREADID tid, pthread_t thread, void **retval)
{
	ThreadLocalStorage* tls = getTLS(tid);
	tls->lastPthreadId = thread;
}

VOID AfterJoin(THREADID tid, int returnValue)
{
	//  dump the current values
	ThreadLocalStorage* tls = getTLS(tid);

	GetLock(&rdmLock, tid + 1);
	printSignatures();

#ifdef PRINT_SYNC_FUNCTION

	fprintf(tls->out, "--- PTHREAD JOIN ---\n");
#endif

	// add current signature to the rdm
	rdm.addSignature(new SigRaceData(tid, *tls->vectorClock,
	                                 *tls->readBloomFilter, *tls->writeBloomFilter));

	tls->vectorClock->advance();
	tls->readBloomFilter->clear();
	tls->writeBloomFilter->clear();
	ReleaseLock(&rdmLock);

	GetLock(&threadIdMapLock, tid + 1);
	PthreadPinIdMapItr itr = pthreadPinIdMap.find(tls->lastPthreadId);
	assert(itr != pthreadPinIdMap.end());
	THREADID joinTID = itr->second;

	printf("JOIN %d <--- %d\n", tid, joinTID); // HERE

	ChildVCMapItr childItr = tls->joinVCMap.find(joinTID);
	assert(childItr != tls->joinVCMap.end());
	ReleaseLock(&threadIdMapLock);

	tls->vectorClock->receiveAction(childItr->second);
}

VOID BeforeLock(THREADID tid, ADDRINT lockAddr)
{
	ThreadLocalStorage* tls = getTLS(tid);
	tls->lockAddr = lockAddr;
}

VOID AfterLock(THREADID tid, int returnValue)
{
	ThreadLocalStorage* tls = getTLS(tid);
	ADDRINT lockAddr = tls->lockAddr;

	if (lockAddr > MUTEX_POINTER_LIMIT)
	{
		return;
	}

	PrintRecordInfo(tid, LOCK);

	VectorClock* vectorClock = tls->vectorClock;
	Bloom* readFilter = tls->readBloomFilter;
	Bloom* writeFilter = tls->writeBloomFilter;

#ifdef DEBUG_MODE

	printf("Thread %d acquired a lock[%lX].\n", tid, lockAddr);
	fflush(stdout);
#endif

	GetLock(&rdmLock, tid + 1);

	printSignatures();
	// add signature to the rdm
	rdm.addSignature(
	    new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));

#ifdef PRINT_SYNC_FUNCTION

	FILE* out = tls->out;
	fprintf(out, "--- MUTEX LOCK ---\n");
#endif

	// increment timestamp
	vectorClock->advance();

	// get the signalling thread and update my timestamp
	UnlockThreadIterator itr = unlockedThreadMap->find(lockAddr);
	if (itr != unlockedThreadMap->end())
	{
		ThreadInfo threadInfo = itr->second;
		if (threadInfo.tid != NO_ID)
		{
			vectorClock->receiveAction(threadInfo.vectorClock);
		}

		// remove the notify signal
		UnlockThreadIterator stiItr = unlockedThreadMap->find(lockAddr);
		stiItr->second.tid = NO_ID;
	}

	ReleaseLock(&rdmLock);

	readFilter->clear();
	writeFilter->clear();
}

VOID AfterTryLock(THREADID tid, int returnValue)
{
	// if we got the lock, the procedure is the same as lock
	if (returnValue == 0)
	{
		AfterLock(tid, returnValue);
	}
}

VOID BeforeUnlock(THREADID tid, ADDRINT lockAddr)
{
	if (lockAddr > MUTEX_POINTER_LIMIT)
	{
		return;
	}

	PrintRecordInfo(tid, UNLOCK);

	ThreadLocalStorage* tls = getTLS(tid);
	VectorClock* vectorClock = tls->vectorClock;
	Bloom* readFilter = tls->readBloomFilter;
	Bloom* writeFilter = tls->writeBloomFilter;

#ifdef DEBUG_MODE

	printf("Thread %d released a lock[%lX].\n", tid, lockAddr);
	fflush(stdout);
#endif

	GetLock(&rdmLock, tid + 1);

	printSignatures();
	// add current signature to the rdm
	rdm.addSignature(
	    new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));

#ifdef PRINT_SYNC_FUNCTION

	FILE* out = tls->out;
	fprintf(out, "--- MUTEX UNLOCK ---\n");
#endif

	// increment timestamp
	vectorClock->advance();
	// update the unlocked map with my vector clock
	(*unlockedThreadMap)[lockAddr].update(tid, *vectorClock);

	ReleaseLock(&rdmLock);

	readFilter->clear();
	writeFilter->clear();
}

VOID BeforeCondWait(THREADID tid, ADDRINT condVarAddr, ADDRINT lockAddr)
{
	ThreadLocalStorage* tls = getTLS(tid);
	VectorClock* vectorClock = tls->vectorClock;
	Bloom* readFilter = tls->readBloomFilter;
	Bloom* writeFilter = tls->writeBloomFilter;

	tls->lockAddr = lockAddr;
	tls->condVarAddr = condVarAddr;

	if (lockAddr > MUTEX_POINTER_LIMIT)
	{
		return;
	}

	PrintRecordInfo(tid, COND_WAIT);

#ifdef DEBUG_MODE

	printf(
	    "Thread %d began waiting on condition variable[%lX] with lock[%lX].\n",
	    tid, condVarAddr, lockAddr);
	fflush(stdout);
#endif

	GetLock(&rdmLock, tid + 1);

	printSignatures();
	// add current signature to the rdm
	rdm.addSignature(
	    new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));

#ifdef PRINT_SYNC_FUNCTION

	FILE* out = tls->out;
	fprintf(out, "--- CONDITION WAIT ---\n");
#endif

	// increment timestamp
	vectorClock->advance();
	// update the unlocked map with my vector clock
	(*unlockedThreadMap)[lockAddr].update(tid, *vectorClock);

	ReleaseLock(&rdmLock);

	readFilter->clear();
	writeFilter->clear();
}

VOID AfterCondWait(THREADID tid, int returnValue)
{
	ThreadLocalStorage* tls = getTLS(tid);
	ADDRINT lockAddr = tls->lockAddr;
	ADDRINT condVarAddr = tls->condVarAddr;

	if (lockAddr > MUTEX_POINTER_LIMIT)
	{
		return;
	}

	PrintRecordInfo(tid, COND_WAIT);

	VectorClock* vectorClock = tls->vectorClock;
	Bloom* readFilter = tls->readBloomFilter;
	Bloom* writeFilter = tls->writeBloomFilter;

#ifdef DEBUG_MODE

	printf(
	    "Thread %d finished waiting on condition variable[%lX] with lock[%lX].\n",
	    tid, condVarAddr, lockAddr);
	fflush(stdout);
#endif

	GetLock(&rdmLock, tid + 1);

#ifdef PRINT_SYNC_FUNCTION

	FILE* out = tls->out;
	fprintf(out, "--- COND WAKE UP ---\n");
#endif

	// get the signalling thread and update my timestamp
	NotifyThreadIterator itr = notifiedThreadMap->find(condVarAddr);
	if (itr != unlockedThreadMap->end())
	{
		ThreadInfo threadInfo = itr->second;

		if (threadInfo.tid != NO_ID)
		{
			vectorClock->receiveAction(threadInfo.vectorClock);
			threadInfo.tid = NO_ID;
		}
	}

	ReleaseLock(&rdmLock);

	readFilter->clear();
	writeFilter->clear();
}

VOID BeforeBarrierInit(THREADID tid, ADDRINT barrier, ADDRINT barrierAttr, int size)
{
	GetLock(&barrierLock, tid + 1);

	// find the barrier list to wait
	BarrierMapItr barrierQueueItr = barrierWaitMap.find((ADDRINT) barrier);

	BarrierData* barrierData = NULL;

	if (barrierQueueItr != barrierWaitMap.end())
	{
		barrierData = barrierQueueItr->second;
	}

	if (barrierData != NULL)
	{
		// remove the previous entry (if any)
		delete barrierData;
	}

	// add the new barrier to the map
	barrierData = new BarrierData(size);
	barrierWaitMap[barrier] = barrierData;

	ReleaseLock(&barrierLock);
}

VOID BeforeBarrierWait(THREADID tid, ADDRINT barrier)
{
	PrintRecordInfo(tid, BARRIER_WAIT);

	ThreadLocalStorage* tls = getTLS(tid);
	tls->barrierAddr = barrier;

	GetLock(&barrierLock, tid + 1);

	// find the barrier list to wait
	BarrierMapItr barrierQueueItr = barrierWaitMap.find(barrier);

	BarrierData* barrierData = NULL;

	if (barrierQueueItr != barrierWaitMap.end())
	{
		barrierData = barrierQueueItr->second;
	}
	assert(barrierData);

	printSignatures();
	// add current signature to the rdm
	rdm.addSignature(
	    new SigRaceData(tid, *(tls->vectorClock), *(tls->readBloomFilter),
	                    *(tls->writeBloomFilter)));

#ifdef PRINT_SYNC_FUNCTION

	fprintf(tls->out, "--- BEFORE BARRIER WAIT ---\n");
#endif

	/*
	 * Advance the vector clock before putting it into the queue
	 * for less number of increments as a whole
	 */
	tls->vectorClock->advance();

#ifdef DEBUG_MODE

	printf("%d is entering into the barrier %ld\n", tid, barrier);
	tls->vectorClock->printVector(stdout);
	fflush(stdout);
#endif

	// update the shared vector clock between waiting threads
	barrierData->vectorClock.receiveAction(*(tls->vectorClock));

	if (++(barrierData->waiterCount) == barrierData->barrierSize)
	{
		// if this is the last thread, handle the bookkeeping
		barrierData->waiterCount = 0;
		barrierData->previousVectorClock = barrierData->vectorClock;
		barrierData->vectorClock.clear();
	}

	ReleaseLock(&barrierLock);

	tls->readBloomFilter->clear();
	tls->writeBloomFilter->clear();

}

VOID AfterBarrierWait(THREADID tid, int returnValue)
{
	// an error occured while waiting in the barrier
	if (returnValue != 0 && returnValue != PTHREAD_BARRIER_SERIAL_THREAD)
	{
		return;
	}

	PrintRecordInfo(tid, BARRIER_WAIT);

	ThreadLocalStorage* tls = getTLS(tid);
	ADDRINT barrierAddr = (ADDRINT) tls->barrierAddr;

	GetLock(&barrierLock, tid + 1);

	// find the correct barrier waiting queue
	BarrierMapItr barrierMapItr = barrierWaitMap.find(barrierAddr);
	BarrierData* barrierData = NULL;
	if (barrierMapItr != barrierWaitMap.end())
	{
		barrierData = barrierMapItr->second;
	}
	assert(barrierData != NULL);

	tls->vectorClock->receiveAction(barrierData->previousVectorClock);

#ifdef DEBUG_MODE

	printf("%d is exiting after the barrier %ld\n", tid, barrierAddr);
	tls->vectorClock->printVector(stdout);
	fflush(stdout);
#endif

	ReleaseLock(&barrierLock);

}

VOID BeforeCondSignal(THREADID tid, ADDRINT condVarAddr)
{
	if (condVarAddr > MUTEX_POINTER_LIMIT)
	{
		return;
	}

	PrintRecordInfo(tid, COND_SIGNAL);

	ThreadLocalStorage* tls = getTLS(tid);
	VectorClock* vectorClock = tls->vectorClock;
	Bloom* readFilter = tls->readBloomFilter;
	Bloom* writeFilter = tls->writeBloomFilter;

#ifdef DEBUG_MODE

	printf("Thread %d signalled a condition variable[%lX].\n", tid,
	       condVarAddr);
	fflush(stdout);
#endif

	GetLock(&rdmLock, tid + 1);

	printSignatures();
	// add current signature to the rdm
	rdm.addSignature(
	    new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));

#ifdef PRINT_SYNC_FUNCTION

	FILE* out = tls->out;
	fprintf(out, "--- SIGNAL ---\n");
#endif

	// increment timestamp
	vectorClock->advance();

	(*notifiedThreadMap)[condVarAddr].update(tid, *vectorClock);

	ReleaseLock(&rdmLock);

	readFilter->clear();
	writeFilter->clear();
}

VOID BeforeCondBroadcast(THREADID tid, ADDRINT condVarAddr)
{
	PrintRecordInfo(tid, COND_BROADCAST);
	BeforeCondSignal(condVarAddr, tid);
}

/*
 * Used to add instrumentation routines before and/or after a function.
 *
 * THREADID & all parameters are passed to the before routines.
 * THREADID & exit value are passed to the after routines.
 *
 * funptr:         function to add instrumentation
 * position:       before, after or both
 * parameterCount: # parameters to pass to the before instrumentation function
 *
 */
static VOID
addInstrumentation(IMG img, const char * name, char position, int parameterCount, AFUNPTR beforeFUNPTR, AFUNPTR afterFUNPTR)
{
	RTN rtn = RTN_FindByName(img, name);
	if (RTN_Valid(rtn))
	{
		RTN_Open(rtn);

		if(position & INSTRUMENT_BEFORE)
		{
			assert(beforeFUNPTR);

			assert(parameterCount >=0 && parameterCount <=4);

			switch(parameterCount)
			{
			case 0:
				RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(beforeFUNPTR),
				               IARG_THREAD_ID,
				               ARGUMENT_LIST_0);
				break;
			case 1:
				RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(beforeFUNPTR),
				               IARG_THREAD_ID,
				               ARGUMENT_LIST_1);
				break;
			case 2:
				RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(beforeFUNPTR),
				               IARG_THREAD_ID,
				               ARGUMENT_LIST_2);
				break;
			case 3:
				RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(beforeFUNPTR),
				               IARG_THREAD_ID,
				               ARGUMENT_LIST_3);
				break;
			case 4:
				RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(beforeFUNPTR),
				               IARG_THREAD_ID,
				               ARGUMENT_LIST_4);
				break;
			default:
				fprintf(stderr,
				        "Instrumentation for functions with %d arguments are not implemented yet\n",
				        parameterCount);
				exit(1);
			}
		}

		if(position & INSTRUMENT_AFTER)
		{
			assert(afterFUNPTR);

			RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(afterFUNPTR),
			               IARG_THREAD_ID,
			               IARG_FUNCRET_EXITPOINT_VALUE,
			               IARG_END);
		}

		RTN_Close(rtn);
	}
}

AFUNPTR
replaceSignature(IMG img, const char* originalFunctionName,
                 AFUNPTR replacementFunction, int functionNo)
{
	RTN rtn = RTN_FindByName(img, originalFunctionName);
	if (RTN_Valid(rtn))
	{
		switch(functionNo)
		{
		case __PTHREAD_CREATE__:
			return RTN_ReplaceSignature(rtn, replacementFunction, PTHREAD_CREATE_ARGS);
		default:
			return NULL;
		}
	}
	return NULL;
}

// This routine is executed for each image.
VOID ImageLoad(IMG img, VOID *)
{
	RTN rtn = RTN_FindByName(img, "INSTRUMENT_OFF");
	if (RTN_Valid(rtn))
	{
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR) TurnInstrumentationOff,
		               IARG_THREAD_ID, IARG_END);
		RTN_Close(rtn);
	}

	rtn = RTN_FindByName(img, "INSTRUMENT_ON");
	if (RTN_Valid(rtn))
	{
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR) TurnInstrumentationOn,
		               IARG_THREAD_ID, IARG_END);
		RTN_Close(rtn);
	}

	if ((IMG_Name(img).find("libpthread.so") != string::npos)
	        || (IMG_Name(img).find("LIBPTHREAD.SO") != string::npos)
	        || (IMG_Name(img).find("LIBPTHREAD.so") != string::npos))
	{
		replaceSignature(img, "pthread_create",
		                 AFUNPTR(PthreadCreateReplacement),
		                 __PTHREAD_CREATE__);

		addInstrumentation(img, "pthread_join", INSTRUMENT_BOTH, 2,
		                   AFUNPTR(BeforeJoin), AFUNPTR(AfterJoin));

		// lock, try-lock, unlock

		addInstrumentation(img, "pthread_mutex_lock", INSTRUMENT_BOTH, 1,
		                   AFUNPTR(BeforeLock), AFUNPTR(AfterLock));

		addInstrumentation(img, "pthread_mutex_trylock", INSTRUMENT_BOTH, 1,
		                   AFUNPTR(BeforeLock), AFUNPTR(AfterTryLock));

		addInstrumentation(img, "pthread_mutex_unlock", INSTRUMENT_BEFORE, 1,
		                   AFUNPTR(BeforeUnlock), NULL);

		// wait, signal

		addInstrumentation(img, "pthread_cond_wait", INSTRUMENT_BOTH, 2,
		                   AFUNPTR(BeforeCondWait), AFUNPTR(AfterCondWait));

		addInstrumentation(img, "pthread_cond_signal", INSTRUMENT_BEFORE, 1,
		                   AFUNPTR(BeforeCondSignal), NULL);

		addInstrumentation(img, "pthread_cond_broadcast", INSTRUMENT_BEFORE, 1,
		                   AFUNPTR(BeforeCondBroadcast), NULL);

		// barrier

		addInstrumentation(img, "pthread_barrier_init", INSTRUMENT_BEFORE, 3,
		                   AFUNPTR(BeforeBarrierInit), NULL);

		addInstrumentation(img, "pthread_barrier_wait", INSTRUMENT_BOTH, 1,
		                   AFUNPTR(BeforeBarrierWait), AFUNPTR(AfterBarrierWait));
	}
}

VOID Fini(INT32 code, VOID *v)
{
	std::sort(threadCreateOrder.begin(), threadCreateOrder.end());
	ThreadCreateOrderItr itr = threadCreateOrder.begin();
	ThreadCreateOrderItr end = threadCreateOrder.end();

	for(; itr != end; itr++)
	{
		CreateInfo ci = *itr;
		fprintf(createFile, "%d %d\n", ci.tid, ci.parent);
	}
	fclose(createFile);
}
