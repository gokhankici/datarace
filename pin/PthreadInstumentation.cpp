#include "PthreadInstumentation.h"
#include "GlobalVariables.h"

#include <assert.h>

/*static void writeInHex(const unsigned char* data, int len)
 {
 for (int i = 0; i < len; i++)
 {
 printf("%02X", data[i]);
 }
 }*/

static ThreadLocalStorage* getTLS(THREADID tid)
{
	ThreadLocalStorage* tls =
			static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	return tls;
}

static void printSignatures(THREADID tid)
{
#ifndef DEBUG_MODE
	return;
#endif

	ThreadLocalStorage* tls = getTLS(tid);
	FILE* out = tls->out;
	VectorClock* vectorClock = tls->vectorClock;

	printf("Thread %d:\n", tid);

	printf("\tVC: ");
	cout << *vectorClock << endl;

	fprintf(out, "--- VC: \n");
	vectorClock->printVector(out);
	fprintf(out, "---\n");
	fflush(stdout);
}

static void printSignatures()
{
	return printSignatures(PIN_ThreadId());
}

// This routine is executed every time a thread is created.
VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
	GetLock(&lock, tid + 1);
	++globalId;
	rdm.addProcessor();
	ReleaseLock(&lock);

	ThreadLocalStorage* tls = new ThreadLocalStorage();

	tls->readBloomFilter = new Bloom();
	tls->writeBloomFilter = new Bloom();

	/* Find the parent and acquire the vector clock info from it */

	GetLock(&threadIdMapLock, tid + 1);

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

		// get its vector clock
		ThreadLocalStorage* parentTls = getTLS(parentTID);

		// create vs from parent's clock
		VectorClock* parentVC = parentTls->vectorClock;
		Bloom* parentRead = parentTls->readBloomFilter;
		Bloom* parentWrite = parentTls->writeBloomFilter;

		// add parent's read/write info to the module
		GetLock(&lock, tid + 1);
		tls->vectorClock = new VectorClock(*parentVC, (int) tid);

		// add current signature to the rdm
		printSignatures(parentTID);
		rdm.addSignature(
				new SigRaceData(parentTID, *parentVC, *parentRead,
						*parentWrite));
		fprintf(parentTls->out, "--- PTHREAD CREATE %d---\n", tid);

		parentVC->advance();
		ReleaseLock(&lock);
	}
	else
	{
		tls->vectorClock = new VectorClock(tid);
	}

	ReleaseLock(&threadIdMapLock);

	string filename = KnobOutputFile.Value() + "." + decstr(tid);
	FILE* out = fopen(filename.c_str(), "w");
	fprintf(out, "thread begins... PIN_TID:%d OS_TID:0x%x\n", tid,
			PIN_GetTid());
	fflush(out);
	tls->out = out;

	assert(tid < MAX_VC_SIZE);

	PIN_SetThreadData(tlsKey, tls, tid);
}

// This routine is executed every time a thread is destroyed.
VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
	ThreadLocalStorage* tls = getTLS(tid);
	FILE* out = tls->out;
	VectorClock* vectorClock = tls->vectorClock;
	Bloom* readFilter = tls->readBloomFilter;
	Bloom* writeFilter = tls->writeBloomFilter;

	GetLock(&lock, tid + 1);
	printSignatures();
	rdm.addSignature(
			new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));
	ReleaseLock(&lock);

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

		GetLock(&lock, tid + 1);

		// save parent's current situation
		printSignatures(parentTID);
		rdm.addSignature(
				new SigRaceData(parentTID, *parentVC, *parentRead,
						*parentWrite));
		fprintf(parentTls->out, "--- PTHREAD JOIN WITH %d---\n", tid);

		// update parent's vc
		parentVC->receiveAction(*vectorClock);
		parentVC->advance();

		ReleaseLock(&lock);
	}

	ReleaseLock(&threadIdMapLock);

	fclose(out);
	delete tls;

	PIN_SetThreadData(tlsKey, 0, tid);
}

VOID BeforePthreadCreate(pthread_t* ptid, THREADID tid)
{
}

VOID AfterPthreadCreate(THREADID tid)
{
}

VOID BeforePthreadJoin(pthread_t ptid, THREADID tid)
{
}

VOID AfterPthreadJoin(THREADID tid)
{
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

	FILE* out = tls->out;
	VectorClock* vectorClock = tls->vectorClock;
	Bloom* readFilter = tls->readBloomFilter;
	Bloom* writeFilter = tls->writeBloomFilter;

#ifdef DEBUG_MODE
	printf("Thread %d acquired a lock[%lX].\n", tid, lockAddr);
	fflush(stdout);
#endif

	GetLock(&lock, tid + 1);

	printSignatures();
	// add signature to the rdm
	rdm.addSignature(
			new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));
	fprintf(out, "--- MUTEX LOCK ---\n");

	// increment timestamp
	vectorClock->advance();

	// get the signalling thread and update my timestamp
	UnlockThreadIterator itr = unlockedThreadMap->find(lockAddr);
	if (itr != unlockedThreadMap->end())
	{
		ThreadInfo threadInfo = itr->second;
		if (threadInfo.tid != NO_ID )
		{
			vectorClock->receiveAction(threadInfo.vectorClock);
		}

		// remove the notify signal
		UnlockThreadIterator stiItr = unlockedThreadMap->find(lockAddr);
		stiItr->second.tid = NO_ID;
	}

	ReleaseLock(&lock);

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

	ThreadLocalStorage* tls = getTLS(tid);
	FILE* out = tls->out;
	VectorClock* vectorClock = tls->vectorClock;
	Bloom* readFilter = tls->readBloomFilter;
	Bloom* writeFilter = tls->writeBloomFilter;

#ifdef DEBUG_MODE
	printf("Thread %d released a lock[%lX].\n", tid, lockAddr);
	fflush(stdout);
#endif

	GetLock(&lock, tid + 1);

	printSignatures();
	// add current signature to the rdm
	rdm.addSignature(
			new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));
	fprintf(out, "--- MUTEX UNLOCK ---\n");

	// increment timestamp
	vectorClock->advance();
	// update the unlocked map with my vector clock
	(*unlockedThreadMap)[lockAddr].update(tid, *vectorClock);

	ReleaseLock(&lock);

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

#ifdef DEBUG_MODE
	printf(
			"Thread %d began waiting on condition variable[%lX] with lock[%lX].\n",
			tid, condVarAddr, lockAddr);
	fflush(stdout);
#endif

	GetLock(&lock, tid + 1);

	printSignatures();
	// add current signature to the rdm
	rdm.addSignature(
			new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));
	fprintf(out, "--- CONDITION WAIT ---\n");

	// increment timestamp
	vectorClock->advance();
	// update the unlocked map with my vector clock
	(*unlockedThreadMap)[lockAddr].update(tid, *vectorClock);

	ReleaseLock(&lock);

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

	GetLock(&lock, tid + 1);

	fprintf(out, "--- COND WAKE UP ---\n");

	// get the signalling thread and update my timestamp
	NotifyThreadIterator itr = notifiedThreadMap->find(condVarAddr);
	if (itr != unlockedThreadMap->end())
	{
		ThreadInfo threadInfo = itr->second;

		if (threadInfo.tid != NO_ID )
		{
			vectorClock->receiveAction(threadInfo.vectorClock);
			threadInfo.tid = NO_ID;
		}
	}

	ReleaseLock(&lock);

	readFilter->clear();
	writeFilter->clear();
}

VOID BeforeBarrierWait(ADDRINT barrierAddr, THREADID tid, char* imageName,
		ADDRINT stackPtr)
{
	ThreadLocalStorage* tls = getTLS(tid);
	tls->barrierAddr = barrierAddr;

	GetLock(&barrierLock, tid + 1);

	// find the barrier list to wait
	BarrierQueueMapIterator barrierQueueItr = barrierWaitMap.find(barrierAddr);
	BarrierQueue* barrierQueue = NULL;
	if (barrierQueueItr != barrierWaitMap.end())
	{
		barrierQueue = barrierQueueItr->second;
	}

	printSignatures();
	// add current signature to the rdm
	rdm.addSignature(
			new SigRaceData(tid, *(tls->vectorClock), *(tls->readBloomFilter),
					*(tls->writeBloomFilter)));
	fprintf(tls->out, "--- BEFORE BARRIER WAIT ---\n");

	/*
	 * Advance the vector clock before putting it into the queue
	 * for less number of increments as a whole
	 */
	tls->vectorClock->advance();

#ifdef DEBUG_MODE
	printf("%d is entering into the barrier %ld\n", tid, barrierAddr);
	tls->vectorClock->printVector(stdout);
	fflush(stdout);
#endif

	// add my value to the list
	if (barrierQueue != NULL)
	{
		barrierQueue->push_back(new ThreadInfo(tid, *tls->vectorClock));
	}
	else
	{
		// create barrier queue if it doesn't exist
		barrierQueue = new std::list<ThreadInfo*>;
		barrierQueue->push_back(new ThreadInfo(tid, *tls->vectorClock));
		barrierWaitMap[barrierAddr] = barrierQueue;
	}

	ReleaseLock(&barrierLock);

	tls->readBloomFilter->clear();
	tls->writeBloomFilter->clear();

}

VOID AfterBarrierWait(int returnCode, THREADID tid)
{
	bool found = false;
	ThreadLocalStorage* tls = getTLS(tid);
	ADDRINT barrierAddr = tls->barrierAddr;

	GetLock(&barrierLock, tid + 1);

	// find the correct barrier waiting queue
	BarrierQueueMapIterator barrierQueueItr = barrierWaitMap.find(barrierAddr);
	BarrierQueue* barrierQueue = NULL;
	if (barrierQueueItr != barrierWaitMap.end())
	{
		barrierQueue = barrierQueueItr->second;
	}
	assert(barrierQueue != NULL);
	VectorClock myClock;

	// find your entry in the list and remove it
	BarrierQueueIterator bqItr = barrierQueue->begin();
	for (; bqItr != barrierQueue->end(); ++bqItr)
	{
		ThreadInfo* info = *bqItr;
		if (info->tid == tid)
		{
			myClock = info->vectorClock;
			barrierQueue->erase(bqItr);
			found = true;
			delete info;
			break;
		}
	}
	assert(found);

	// an error occured while waiting in the barrier
	if (returnCode != 0 && returnCode != PTHREAD_BARRIER_SERIAL_THREAD)
	{
		ReleaseLock(&barrierLock);
		return;
	}

	// handshake with all other barrier waiters
	bqItr = barrierQueue->begin();
	for (; bqItr != barrierQueue->end(); ++bqItr)
	{
		ThreadInfo* info = *bqItr;
		myClock.receiveAction(info->vectorClock);
		info->vectorClock.receiveAction(myClock);
	}

	// update the clock in TLS
	*(tls->vectorClock) = myClock;

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

	GetLock(&lock, tid + 1);

	printSignatures();
	// add current signature to the rdm
	rdm.addSignature(
			new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));
	fprintf(out, "--- SIGNAL ---\n");

	// increment timestamp
	vectorClock->advance();

	(*notifiedThreadMap)[condVarAddr].update(tid, *vectorClock);

	ReleaseLock(&lock);

	readFilter->clear();
	writeFilter->clear();
}

VOID BeforeCondBroadcast(ADDRINT condVarAddr, THREADID tid)
{
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
}
