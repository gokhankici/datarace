#ifndef _PTHREAD_INSTRUMENTATION_H_
#define _PTHREAD_INSTRUMENTATION_H_

#include "pin.H"

#include "MultiCacheSim_PinDriver.h"
#include "SigraceModules.h"
#include "Bloom.h"

extern WaitQueueMap* waitQueueMap;
extern SignalThreadMap* signalledThreadMap;
extern RaceDetectionModule rdm;

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
VOID ImageLoad (IMG img, VOID *);
VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v);
VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v);
VOID BeforeLock (ADDRINT lockAddr, THREADID tid);
VOID AfterLock (THREADID tid);
VOID BeforeUnlock (ADDRINT lockAddr, THREADID tid);
VOID BeforePthreadCreate(ADDRINT lockAddr, THREADID id ,char* imageName, ADDRINT stackPtr);
VOID BeforePthreadJoin(ADDRINT lockAddr, THREADID id ,char* imageName, ADDRINT stackPtr);
VOID BeforeCondWait(ADDRINT condVarAddr,ADDRINT lockAddr, THREADID id ,char* imageName, ADDRINT stackPtr);
VOID AfterCondWait(ADDRINT condVarAddr, THREADID id ,char* imageName, ADDRINT stackPtr);
VOID AfterBarrirerWait(ADDRINT barrierAddr, THREADID id ,char* imageName, ADDRINT stackPtr);
VOID BeforeBarrirerWait(ADDRINT condVarAddr, THREADID id ,char* imageName, ADDRINT stackPtr);
VOID BeforeCondSignal(ADDRINT condVarAddr, THREADID id ,char* imageName, ADDRINT stackPtr);
VOID BeforeCondBroadcast(ADDRINT condVarAddr, THREADID id ,char* imageName, ADDRINT stackPtr);
VOID AfterCondBroadcast(ADDRINT condVarAddr, THREADID id ,char* imageName, ADDRINT stackPtr);

/*INSTRUMENTATION FUNCTIONS - IMPLEMENTATIONS*/
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
	ThreadLocalStorage* tls = static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	VectorClock* vectorClock = tls->vectorClock;
	//Bloom* readSig = static_cast<Bloom*>(PIN_GetThreadData(tlsReadSignatureKey, tid));
	//Bloom* writeSig = static_cast<Bloom*>(PIN_GetThreadData(tlsWriteSignatureKey, tid));

	printf("Thread %d:\n", tid);

	printf("\tVC: ");
	int i = 0;
	for (VectorClock::iterator vci = vectorClock->begin(); vci != vectorClock->end() ; vci++) 
	{
		if(i++ > 2)
			break;

		printf("%d, ", *vci);
	}
	printf("\n\n");

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
	GetLock(&lock, tid+1);
	++globalId;
	rdm.addProcessor();
	ReleaseLock(&lock);

	ThreadLocalStorage* tls = new ThreadLocalStorage();

	Bloom* readBloomFilter = new Bloom();
	Bloom* writeBloomFilter = new Bloom();
	tls->readBloomFilter = readBloomFilter;
	tls->writeBloomFilter = writeBloomFilter;

	string filename = KnobOutputFile.Value() +"." + decstr(tid);
	FILE* out       = fopen(filename.c_str(), "w");
	fprintf(out, "thread begins... PIN_TID:%d OS_TID:0x%x\n",tid,PIN_GetTid());
	fflush(out);
	tls->out = out;

	assert(tid < MAX_VC_SIZE);
	VectorClock* vectorClock = new vector<UINT32>(MAX_VC_SIZE, 0);
	(*vectorClock)[tid]=1;
	tls->vectorClock = vectorClock;

	PIN_SetThreadData(tlsKey, tls, tid);
}

// This routine is executed every time a thread is destroyed.
VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
	ThreadLocalStorage* tls = static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	FILE* out   = tls->out;
	VectorClock* vectorClock = tls->vectorClock;
	Bloom* readFilter = tls->readBloomFilter;
	Bloom* writeFilter = tls->writeBloomFilter;

	GetLock(&lock, tid+1);
	(*vectorClock)[tid]++;
	printSignatures();
	rdm.addSignature(new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));
	ReleaseLock(&lock);

	fclose(out);
	delete tls;

	PIN_SetThreadData(tlsKey, 0, tid);
}

// This routine is executed each time lock is called.
VOID BeforeLock (ADDRINT lockAddr, THREADID tid)
{
	ThreadLocalStorage* tls = static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	/*if(CONVERT(long, lockAddr) > MUTEX_POINTER_LIMIT)
	{
		tls->lockAddr = lockAddr;
		return;
	}*/

	WaitQueue* mutexWaitList = NULL;
	// point to the current mutex
	tls->lockAddr = lockAddr;

	GetLock(&lock, tid+1);

	WaitQueueIterator foundQueueItr = waitQueueMap->find(lockAddr);
	if(foundQueueItr != waitQueueMap->end())
	{
		mutexWaitList = foundQueueItr->second;
	}

	if (mutexWaitList) 
	{
		mutexWaitList->push_back(tid);
	}
	else
	{
		mutexWaitList = new std::list<UINT32>;
		mutexWaitList->push_back(tid);
		(*waitQueueMap)[lockAddr] = mutexWaitList;
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

VOID AfterLock (THREADID tid)
{
	ThreadLocalStorage* tls = static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	WaitQueue* mutexWaitList = NULL;
	FILE* out   = tls->out;
	ADDRINT lockAddr = tls->lockAddr;
	VectorClock* vectorClock = tls->vectorClock;
	Bloom* readFilter = tls->readBloomFilter;
	Bloom* writeFilter = tls->writeBloomFilter;

	/*if(CONVERT(long, mutex) > MUTEX_POINTER_LIMIT)
		return;*/

#ifdef DEBUG_MODE
		printf("Thread %d acquired a lock[%ld].\n", tid, lockAddr);
#endif

	GetLock(&lock, tid+1);

	// add signature to the rdm
	rdm.addSignature(new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));
	fprintf(out, "--- MUTEX LOCK ---\n");

	// increment timestamp
	(*vectorClock)[tid]++;

	// get the signalling thread and update my timestamp
	SignalThreadIterator itr = signalledThreadMap->find(lockAddr);
	if(itr != signalledThreadMap->end())
	{
		SignalThreadInfo signalThreadInfo = itr->second;
		if (signalThreadInfo.tid != NO_ID) 
		{
			updateVectorClock(tid, *vectorClock, signalThreadInfo.vectorClock);
		}

		// remove the notify signal
		SignalThreadIterator stiItr = signalledThreadMap->find(lockAddr);
		stiItr->second.tid = NO_ID;
	}

	// remove myself from the waiting queue
	WaitQueueIterator foundQueueItr = waitQueueMap->find(lockAddr);
	if(foundQueueItr != waitQueueMap->end() && mutexWaitList)
	{
		mutexWaitList = foundQueueItr->second;
		mutexWaitList->remove(tid);
	}
	printSignatures();

	ReleaseLock(&lock);

	readFilter->clear();
	writeFilter->clear();
}

VOID BeforeUnlock (ADDRINT lockAddr, THREADID tid)
{
	/*if(CONVERT(long, lockAddr) > MUTEX_POINTER_LIMIT)
		return;*/

	ThreadLocalStorage* tls = static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	FILE* out   = tls->out;
	VectorClock* vectorClock = tls->vectorClock;
	Bloom* readFilter = tls->readBloomFilter;
	Bloom* writeFilter = tls->writeBloomFilter;

#ifdef DEBUG_MODE
	printf("Thread %d released a lock[%ld].\n", tid, lockAddr);
#endif

	GetLock(&lock, tid+1);

	// add current signature to the rdm
	rdm.addSignature(new SigRaceData(tid, *vectorClock, *readFilter, *writeFilter));
	fprintf(out, "--- MUTEX UNLOCK ---\n");

	// increment timestamp
	(*vectorClock)[tid]++;
	// update the signalled map with my vector clock
	(*signalledThreadMap)[lockAddr].update(tid, *vectorClock);

	printSignatures();
	ReleaseLock(&lock);

	readFilter->clear();
	writeFilter->clear();
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

	rtn = RTN_FindByName(img, "INSTRUMENT_OFF");
	if (RTN_Valid(rtn))
	{
		RTN_Open(rtn);
		RTN_InsertCall(rtn, 
				IPOINT_BEFORE, 
				(AFUNPTR)TurnInstrumentationOff, 
				IARG_THREAD_ID,
				IARG_END);
		RTN_Close(rtn);
	}


	rtn = RTN_FindByName(img, "INSTRUMENT_ON");
	if (RTN_Valid(rtn))
	{
		RTN_Open(rtn);
		RTN_InsertCall(rtn, 
				IPOINT_BEFORE, 
				(AFUNPTR)TurnInstrumentationOn, 
				IARG_THREAD_ID,
				IARG_END);
		RTN_Close(rtn);
	}
}

VOID BeforePthreadCreate(ADDRINT lockAddr, THREADID id ,char* imageName, ADDRINT stackPtr)
{

}

VOID BeforePthreadJoin(ADDRINT lockAddr, THREADID id ,char* imageName, ADDRINT stackPtr)
{

}

VOID BeforeCondWait(ADDRINT condVarAddr,ADDRINT lockAddr, THREADID id ,char* imageName, ADDRINT stackPtr)
{

}

VOID AfterCondWait(ADDRINT condVarAddr, THREADID id ,char* imageName, ADDRINT stackPtr)
{

}

VOID AfterBarrirerWait(ADDRINT barrierAddr, THREADID id ,char* imageName, ADDRINT stackPtr)
{

}

VOID BeforeBarrirerWait(ADDRINT condVarAddr, THREADID id ,char* imageName, ADDRINT stackPtr)
{

}

VOID BeforeCondSignal(ADDRINT condVarAddr, THREADID id ,char* imageName, ADDRINT stackPtr)
{

}

VOID BeforeCondBroadcast(ADDRINT condVarAddr, THREADID id ,char* imageName, ADDRINT stackPtr)
{

}

VOID AfterCondBroadcast(ADDRINT condVarAddr, THREADID id ,char* imageName, ADDRINT stackPtr)
{

}


#endif
