#ifndef _PTHREAD_INSTRUMENTATION_H_
#define _PTHREAD_INSTRUMENTATION_H_

#include "pin.H"

#include "MultiCacheSim_PinDriver.h"
#include "SigraceModules.h"
#include "Bloom.h"

extern UINT32 globalId;
extern PIN_LOCK lock;

//extern WaitQueueMap* waitQueueMap;
extern UnlockThreadMap* unlockedThreadMap;
extern NotifyThreadMap* notifiedThreadMap;
extern RaceDetectionModule rdm;
extern PthreadTidMap tidMap;

extern KNOB<string> KnobOutputFile;
extern KNOB<bool> KnobStopOnError;
extern KNOB<bool> KnobPrintOnError;
extern KNOB<bool> KnobConcise;
extern KNOB<unsigned int> KnobCacheSize;
extern KNOB<unsigned int> KnobBlockSize;
extern KNOB<unsigned int> KnobAssoc;
extern KNOB<unsigned int> KnobNumCaches;
extern KNOB<string> KnobProtocol;
extern KNOB<string> KnobReference;

/*INSTRUMENTATION FUNCTIONS - FORWARD DECLARATIONS*/
VOID ImageLoad(IMG img, VOID *);
VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v);
VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v);
VOID BeforeLock(ADDRINT lockAddr, THREADID tid);
VOID AfterLock(THREADID tid);
VOID BeforeUnlock(ADDRINT lockAddr, THREADID tid);

VOID BeforePthreadCreate(pthread_t* ptid, THREADID id);
VOID AfterPthreadCreate(THREADID tid);
VOID BeforePthreadJoin(pthread_t ptid, THREADID id);
VOID AfterPthreadJoin(THREADID tid);

VOID BeforeCondWait(ADDRINT condVarAddr, ADDRINT lockAddr, THREADID id);
VOID AfterCondWait(THREADID id);

VOID AfterBarrirerWait(ADDRINT barrierAddr, THREADID id);
VOID BeforeBarrirerWait(ADDRINT condVarAddr, THREADID id);

VOID BeforeCondSignal(ADDRINT condVarAddr, THREADID id);
VOID BeforeCondBroadcast(ADDRINT condVarAddr, THREADID id);
VOID AfterCondBroadcast(ADDRINT condVarAddr, THREADID id);

/*static void writeInHex(const unsigned char* data, int len)
 {
 for (int i = 0; i < len; i++)
 {
 printf("%02X", data[i]);
 }
 }*/

static void printSignatures()
{
#ifndef DEBUG_MODE
	return;
#endif

	int tid = PIN_ThreadId();
	ThreadLocalStorage* tls =
			static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	FILE* out = tls->out;
	VectorClock* vectorClock = tls->vectorClock;
	//Bloom* readSig = static_cast<Bloom*>(PIN_GetThreadData(tlsReadSignatureKey, tid));
	//Bloom* writeSig = static_cast<Bloom*>(PIN_GetThreadData(tlsWriteSignatureKey, tid));

	printf("Thread %d:\n", tid);

	printf("\tVC: ");
	fprintf(out, "--- VC: ");
	for (unsigned int i = 0; i < globalId; i++)
	{
		printf("%d, ", (*vectorClock)[i]);
		fprintf(out, "%d, ", (*vectorClock)[i]);
	}
	printf("\n\n");
	fprintf(out, "---\n");
	fflush(stdout);

	//printf("\tR: ");
	//writeInHex(readSig->getFilter(), readSig->getFilterSizeInBytes());
	//printf("\n");

	//printf("\tW: ");
	//writeInHex(writeSig->getFilter(), writeSig->getFilterSizeInBytes());
	//printf("\n");
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

	string filename = KnobOutputFile.Value() + "." + decstr(tid);
	FILE* out = fopen(filename.c_str(), "w");
	fprintf(out, "thread begins... PIN_TID:%d OS_TID:0x%x\n", tid,
			PIN_GetTid());
	fflush(out);
	tls->out = out;

	assert(tid < MAX_VC_SIZE);
	VectorClock* vectorClock = new vector<UINT32>(MAX_VC_SIZE, 0);
	(*vectorClock)[tid] = 1;
	tls->vectorClock = vectorClock;

	PIN_SetThreadData(tlsKey, tls, tid);
}

// This routine is executed every time a thread is destroyed.
VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
	ThreadLocalStorage* tls =
			static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	FILE* out = tls->out;
	VectorClock* vectorClock = tls->vectorClock;
	Bloom* readFilter = tls->readBloomFilter;
	Bloom* writeFilter = tls->writeBloomFilter;

	GetLock(&lock, tid + 1);
	printSignatures();
	rdm.addSignature(
			new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));
	//(*vectorClock)[tid]++;
	ReleaseLock(&lock);

	fclose(out);
	delete tls;

	PIN_SetThreadData(tlsKey, 0, tid);
}

VOID BeforePthreadCreate(pthread_t* ptid, THREADID tid)
{
	//ThreadLocalStorage* tls = static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	//tls->createdThread = ptid;
}

VOID AfterPthreadCreate(THREADID tid)
{

}

VOID BeforePthreadJoin(pthread_t ptid, THREADID tid)
{
	//ThreadLocalStorage* tls = static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	//tls->joinedThread = ptid;
}

VOID AfterPthreadJoin(THREADID tid)
{

}

// This routine is executed each time lock is called.
VOID BeforeLock(ADDRINT lockAddr, THREADID tid)
{
	ThreadLocalStorage* tls =
			static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	tls->lockAddr = lockAddr;

	if (lockAddr > MUTEX_POINTER_LIMIT)
	{
		return;
	}
}

static void updateVectorClock(UINT32 myThreadId, VectorClock& myClock,
		VectorClock& otherClock)
{
	for (unsigned int i = 0; i < MAX_VC_SIZE; i++)
	{
		if (i == myThreadId)
		{
			continue;
		}

		myClock[i] = std::max(myClock[i], otherClock[i]);
	}
}

VOID AfterLock(THREADID tid)
{
	ThreadLocalStorage* tls =
			static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
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
	(*vectorClock)[tid]++;

	// get the signalling thread and update my timestamp
	UnlockThreadIterator itr = unlockedThreadMap->find(lockAddr);
	if (itr != unlockedThreadMap->end())
	{
		ThreadInfo threadInfo = itr->second;
		if (threadInfo.tid != NO_ID )
		{
			updateVectorClock(tid, *vectorClock, threadInfo.vectorClock);
		}

		// remove the notify signal
		UnlockThreadIterator stiItr = unlockedThreadMap->find(lockAddr);
		stiItr->second.tid = NO_ID;
	}

	ReleaseLock(&lock);

	readFilter->clear();
	writeFilter->clear();
}

VOID BeforeUnlock(ADDRINT lockAddr, THREADID tid)
{
	if (lockAddr > MUTEX_POINTER_LIMIT)
	{
		return;
	}

	ThreadLocalStorage* tls =
			static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
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
	(*vectorClock)[tid]++;
	// update the unlocked map with my vector clock
	(*unlockedThreadMap)[lockAddr].update(tid, *vectorClock);

	ReleaseLock(&lock);

	readFilter->clear();
	writeFilter->clear();
}

VOID BeforeCondWait(ADDRINT condVarAddr, ADDRINT lockAddr, THREADID tid)
{
	ThreadLocalStorage* tls =
			static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	//WaitQueue* waitQueue = NULL;
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
	printf("Thread %d began waiting on condition variable[%lX] with lock[%lX].\n", tid, condVarAddr, lockAddr);
	fflush(stdout);
#endif

	GetLock(&lock, tid + 1);

	printSignatures();
	// add current signature to the rdm
	rdm.addSignature(
			new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));
	fprintf(out, "--- CONDITION WAIT ---\n");

	// increment timestamp
	(*vectorClock)[tid]++;
	// update the unlocked map with my vector clock
	(*unlockedThreadMap)[lockAddr].update(tid, *vectorClock);

	/*WaitQueueIterator foundQueueItr = waitQueueMap->find(condVarAddr);
	 if(foundQueueItr != waitQueueMap->end())
	 {
	 waitQueue = foundQueueItr->second;
	 }

	 if (waitQueue)
	 {
	 waitQueue->push_back(tid);
	 }
	 else
	 {
	 waitQueue = new std::list<UINT32>;
	 waitQueue->push_back(tid);
	 (*waitQueueMap)[condVarAddr] = waitQueue;
	 }*/

	ReleaseLock(&lock);

	readFilter->clear();
	writeFilter->clear();
}

VOID AfterCondWait(THREADID tid)
{
	ThreadLocalStorage* tls =
			static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
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
	printf("Thread %d finished waiting on condition variable[%lX] with lock[%lX].\n", tid, condVarAddr, lockAddr);
	fflush(stdout);
#endif

	GetLock(&lock, tid + 1);

	//printSignatures();
	// add signature to the rdm
	//rdm.addSignature(new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));
	fprintf(out, "--- COND WAKE UP ---\n");

	// increment timestamp
	//(*vectorClock)[tid]++;

	// get the signalling thread and update my timestamp
	NotifyThreadIterator itr = notifiedThreadMap->find(condVarAddr);
	if (itr != unlockedThreadMap->end())
	{
		ThreadInfo threadInfo = itr->second;

		if (threadInfo.tid != NO_ID )
		{
			updateVectorClock(tid, *vectorClock, threadInfo.vectorClock);
			threadInfo.tid = NO_ID;
		}
	}

	/*// remove myself from the waiting queue
	 WaitQueueIterator foundQueueItr = waitQueueMap->find(condVarAddr);
	 if(foundQueueItr != waitQueueMap->end())
	 {
	 WaitQueue* waitQueue = foundQueueItr->second;
	 waitQueue->remove(tid);
	 }*/

	ReleaseLock(&lock);

	readFilter->clear();
	writeFilter->clear();
}

VOID AfterBarrirerWait(ADDRINT barrierAddr, THREADID id, char* imageName,
		ADDRINT stackPtr)
{

}

VOID BeforeBarrirerWait(ADDRINT condVarAddr, THREADID id, char* imageName,
		ADDRINT stackPtr)
{

}

VOID BeforeCondSignal(ADDRINT condVarAddr, THREADID tid)
{
	if (condVarAddr > MUTEX_POINTER_LIMIT)
	{
		return;
	}

	ThreadLocalStorage* tls =
			static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	FILE* out = tls->out;
	VectorClock* vectorClock = tls->vectorClock;
	Bloom* readFilter = tls->readBloomFilter;
	Bloom* writeFilter = tls->writeBloomFilter;

#ifdef DEBUG_MODE
	printf("Thread %d signalled a condition variable[%lX].\n", tid, condVarAddr);
	fflush(stdout);
#endif

	GetLock(&lock, tid + 1);

	printSignatures();
	// add current signature to the rdm
	rdm.addSignature(
			new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));
	fprintf(out, "--- SIGNAL ---\n");

	// increment timestamp
	(*vectorClock)[tid]++;

	(*notifiedThreadMap)[condVarAddr].update(tid, *vectorClock);

	ReleaseLock(&lock);

	readFilter->clear();
	writeFilter->clear();
}

VOID BeforeCondBroadcast(ADDRINT condVarAddr, THREADID tid)
{
	if (condVarAddr > MUTEX_POINTER_LIMIT)
	{
		return;
	}

	ThreadLocalStorage* tls =
			static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	FILE* out = tls->out;
	VectorClock* vectorClock = tls->vectorClock;
	Bloom* readFilter = tls->readBloomFilter;
	Bloom* writeFilter = tls->writeBloomFilter;

#ifdef DEBUG_MODE
	printf("Thread %d signalled a condition variable[%lX].\n", tid, condVarAddr);
	fflush(stdout);
#endif

	GetLock(&lock, tid + 1);

	printSignatures();
	// add current signature to the rdm
	rdm.addSignature(
			new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));
	fprintf(out, "--- SIGNAL ---\n");

	// increment timestamp
	(*vectorClock)[tid]++;

	(*notifiedThreadMap)[condVarAddr].update(tid, *vectorClock);

	ReleaseLock(&lock);

	readFilter->clear();
	writeFilter->clear();
}

VOID AfterCondBroadcast(ADDRINT condVarAddr, THREADID id, char* imageName,
		ADDRINT stackPtr)
{

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
}

#endif
