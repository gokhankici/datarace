#include <stdio.h>
#include <assert.h>
#include <algorithm>

#include "PthreadInstumentation.h"
#include "GlobalVariables.h"
#include "RecordNReplay.h"

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
static ThreadLocalStorage* getTLS(THREADID tid)
{
	ThreadLocalStorage* tls =
			static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	return tls;
}

/*
 * Log the timestamp and signatures
 */
static void printSignatures(THREADID tid)
{
	ThreadLocalStorage* tls = getTLS(tid);
	FILE* out = tls->out;
	VectorClock* vectorClock = tls->vectorClock;

#ifdef DEBUG_MODE
	printf("Thread %d:\n", tid);
	printf("\tVC: ");
	cout << *vectorClock << endl;
	fflush(stdout);
#endif

	vectorClock->printVector(out);
	fflush(out);
}

static void printSignatures()
{
	return printSignatures(PIN_ThreadId());
}

/**
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

		VectorClock* parentVC = parentTls->vectorClock;
		Bloom* parentRead = parentTls->readBloomFilter;
		Bloom* parentWrite = parentTls->writeBloomFilter;

		// add parent's read/write info to the module
		GetLock(&rdmLock, tid + 1);
		tls->vectorClock = new VectorClock(*parentVC, (int) tid);

		printSignatures(parentTID);

		// add current signature to the rdm
		rdm.addSignature(
				new SigRaceData(parentTID, *parentVC, *parentRead,
						*parentWrite));

#ifdef PRINT_SYNC_FUNCTION
		fprintf(parentTls->out, "--- PTHREAD CREATE %d---\n", tid);
#endif

		parentVC->advance();
		parentRead->clear();
		parentWrite->clear();

		ReleaseLock(&rdmLock);
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
	rdm.addSignature(
			new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));
	ReleaseLock(&rdmLock);

	vectorClock->advance();

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
		ThreadLocalStorage* parentTls = getTLS(parentTID);

		// create vs from parent's clock
		VectorClock* parentVC = parentTls->vectorClock;
		Bloom* parentRead = parentTls->readBloomFilter;
		Bloom* parentWrite = parentTls->writeBloomFilter;

		GetLock(&rdmLock, tid + 1);

		// save parent's current situation
		printSignatures(parentTID);

		rdm.addSignature(
				new SigRaceData(parentTID, *parentVC, *parentRead,
						*parentWrite));

#ifdef PRINT_SYNC_FUNCTION
		fprintf(parentTls->out, "--- PTHREAD JOIN WITH %d---\n", tid);
#endif

		// update parent's vc
		parentVC->receiveAction(*vectorClock);
		parentVC->advance();
		parentRead->clear();
		parentWrite->clear();

		ReleaseLock(&rdmLock);
	}

	ReleaseLock(&threadIdMapLock);

	fclose(out);
	delete tls;

	PIN_SetThreadData(tlsKey, 0, tid);
}

// This routine is executed each time lock is called.
VOID BeforeLock(ADDRINT lockAddr, THREADID tid)
{
	ThreadLocalStorage* tls = getTLS(tid);
	tls->lockAddr = lockAddr;
}

VOID AfterLock(THREADID tid)
{
	ThreadLocalStorage* tls = getTLS(tid);
	ADDRINT lockAddr = tls->lockAddr;

	if (lockAddr > MUTEX_POINTER_LIMIT)
	{
		return;
	}

	PrintRecordInfo(tid, LOCK);

	FILE* out = tls->out;
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

VOID AfterTryLock(ADDRINT exitVal, THREADID tid)
{
	// if we got the lock, the procedure is the same as lock
	if (exitVal == 0)
	{
		AfterLock(tid);
	}
}

VOID BeforeUnlock(ADDRINT lockAddr, THREADID tid)
{
	if (lockAddr > MUTEX_POINTER_LIMIT)
	{
		return;
	}

	PrintRecordInfo(tid, UNLOCK);

	ThreadLocalStorage* tls = getTLS(tid);
	FILE* out = tls->out;
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

VOID BeforeCondWait(ADDRINT condVarAddr, ADDRINT lockAddr, THREADID tid)
{
	ThreadLocalStorage* tls = getTLS(tid);
	FILE* out = tls->out;
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

VOID AfterCondWait(THREADID tid)
{
	ThreadLocalStorage* tls = getTLS(tid);
	ADDRINT lockAddr = tls->lockAddr;
	ADDRINT condVarAddr = tls->condVarAddr;

	if (lockAddr > MUTEX_POINTER_LIMIT)
	{
		return;
	}

	PrintRecordInfo(tid, COND_WAIT);

	FILE* out = tls->out;
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

VOID BeforeBarrierInit(THREADID tid, ADDRINT barrier, int size)
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

VOID BeforeBarrierWait(ADDRINT barrier, THREADID tid)
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

VOID AfterBarrierWait(int returnCode, THREADID tid)
{
	// an error occured while waiting in the barrier
	if (returnCode != 0 && returnCode != PTHREAD_BARRIER_SERIAL_THREAD)
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

VOID BeforeCondSignal(ADDRINT condVarAddr, THREADID tid)
{
	if (condVarAddr > MUTEX_POINTER_LIMIT)
	{
		return;
	}

	PrintRecordInfo(tid, COND_SIGNAL);

	ThreadLocalStorage* tls = getTLS(tid);
	FILE* out = tls->out;
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
	fprintf(out, "--- SIGNAL ---\n");
#endif

	// increment timestamp
	vectorClock->advance();

	(*notifiedThreadMap)[condVarAddr].update(tid, *vectorClock);

	ReleaseLock(&rdmLock);

	readFilter->clear();
	writeFilter->clear();
}

VOID BeforeCondBroadcast(ADDRINT condVarAddr, THREADID tid)
{
	PrintRecordInfo(tid, COND_BROADCAST);
	BeforeCondSignal(condVarAddr, tid);
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

	/* MUTEX LOCK / UNLOCK */
	rtn = RTN_FindByName(img, "pthread_mutex_lock");
	if (RTN_Valid(rtn))
	{
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(BeforeLock),
				IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_THREAD_ID, IARG_END);
		RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(AfterLock), IARG_THREAD_ID,
		IARG_END);
		RTN_Close(rtn);
	}

	rtn = RTN_FindByName(img, "pthread_mutex_trylock");
	if (RTN_Valid(rtn))
	{
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(BeforeLock),
				IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_THREAD_ID, IARG_END);
		RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(AfterTryLock),
				IARG_FUNCRET_EXITPOINT_VALUE, IARG_THREAD_ID, IARG_END);
		RTN_Close(rtn);
	}

	rtn = RTN_FindByName(img, "pthread_mutex_unlock");
	if (RTN_Valid(rtn))
	{
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(BeforeUnlock),
				IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_THREAD_ID, IARG_END);
		RTN_Close(rtn);
	}

	/* WAIT / NOTIFY */

	rtn = RTN_FindByName(img, "pthread_cond_wait");
	if (RTN_Valid(rtn))
	{
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(BeforeCondWait),
				IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE,
				1, IARG_THREAD_ID, IARG_END);
		RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(AfterCondWait),
				IARG_THREAD_ID, IARG_END);
		RTN_Close(rtn);
	}

	rtn = RTN_FindByName(img, "pthread_cond_signal");
	if (RTN_Valid(rtn))
	{
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(BeforeCondSignal),
				IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_THREAD_ID, IARG_END);
		RTN_Close(rtn);
	}

	rtn = RTN_FindByName(img, "pthread_cond_broadcast");
	if (RTN_Valid(rtn))
	{
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(BeforeCondBroadcast),
				IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_THREAD_ID, IARG_END);
		RTN_Close(rtn);
	}

	rtn = RTN_FindByName(img, "pthread_barrier_wait");
	if (RTN_Valid(rtn))
	{
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(BeforeBarrierWait),
				IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_THREAD_ID, IARG_END);
		RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(AfterBarrierWait),
				IARG_FUNCRET_EXITPOINT_VALUE, IARG_THREAD_ID, IARG_END);
		RTN_Close(rtn);
	}

	rtn = RTN_FindByName(img, "pthread_barrier_init");
	if (RTN_Valid(rtn))
	{
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(BeforeBarrierInit),
				IARG_THREAD_ID, IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
				IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_END);
		RTN_Close(rtn);
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
