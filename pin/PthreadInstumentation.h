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

//static void writeInHex(const unsigned char* data, int len)
//{
	//for (int i = 0; i < len; i++) 
	//{
		//printf("%02X", data[i]);
	//}
//}

static void printSignatures()
{
#ifndef DEBUG_MODE
		return;
#endif

	int tid = PIN_ThreadId();
	VectorClock* vectorClock = static_cast<VectorClock*>(PIN_GetThreadData(vectorClockKey, tid));
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
VOID ThreadStart(THREADID threadId, CONTEXT *ctxt, INT32 flags, VOID *v)
{
	GetLock(&lock, threadId+1);
	++globalId;
	rdm.addProcessor();
	ReleaseLock(&lock);

	Bloom* readBloomFilter = new Bloom();
	PIN_SetThreadData(tlsReadSignatureKey, readBloomFilter, threadId);
	Bloom* writeBloomFilter = new Bloom();
	PIN_SetThreadData(tlsWriteSignatureKey, writeBloomFilter, threadId);

	string filename = KnobOutputFile.Value() +"." + decstr(threadId);
	FILE* out       = fopen(filename.c_str(), "w");
	fprintf(out, "thread begins... PIN_TID:%d OS_TID:0x%x\n",threadId,PIN_GetTid());
	fflush(out);
	PIN_SetThreadData(tlsKey, out, threadId);
	PIN_SetThreadData(mutexPtrKey, 0, threadId);

	assert(threadId < MAX_VC_SIZE);
	VectorClock* vectorClock = new vector<UINT32>(MAX_VC_SIZE, 0);
	(*vectorClock)[threadId]=1;
	PIN_SetThreadData(vectorClockKey, vectorClock, threadId);
}

// This routine is executed every time a thread is destroyed.
VOID ThreadFini(THREADID threadId, const CONTEXT *ctxt, INT32 code, VOID *v)
{
	FILE* out   = static_cast<FILE*>(PIN_GetThreadData(tlsKey, threadId));
	VectorClock* vectorClock = static_cast<VectorClock*>(PIN_GetThreadData(vectorClockKey, threadId));
	Bloom* readFilter = static_cast<Bloom*>(PIN_GetThreadData(tlsReadSignatureKey, threadId));
	Bloom* writeFilter = static_cast<Bloom*>(PIN_GetThreadData(tlsWriteSignatureKey, threadId));

	GetLock(&lock, threadId+1);
	(*vectorClock)[threadId]++;
	printSignatures();
	rdm.addSignature(new SigRaceData(threadId, *vectorClock, *readFilter, *writeFilter));
	ReleaseLock(&lock);

	delete vectorClock;
	delete readFilter;
	delete writeFilter;

	fclose(out);
	PIN_SetThreadData(tlsKey, 0, threadId);
	PIN_SetThreadData(mutexPtrKey, 0, threadId);
	PIN_SetThreadData(vectorClockKey, 0, threadId);
	PIN_SetThreadData(tlsReadSignatureKey, 0, threadId);
	PIN_SetThreadData(tlsWriteSignatureKey, 0, threadId);
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
	FILE* out   = static_cast<FILE*>(PIN_GetThreadData(tlsKey, threadId));
	pthread_mutex_t* mutex = static_cast<pthread_mutex_t*>(PIN_GetThreadData(mutexPtrKey, threadId));
	VectorClock* vectorClock = static_cast<VectorClock*>(PIN_GetThreadData(vectorClockKey, threadId));
	Bloom* readFilter = static_cast<Bloom*>(PIN_GetThreadData(tlsReadSignatureKey, threadId));
	Bloom* writeFilter = static_cast<Bloom*>(PIN_GetThreadData(tlsWriteSignatureKey, threadId));
	if(CONVERT(long, mutex) > MUTEX_POINTER_LIMIT)
		return;

#ifdef DEBUG_MODE
		printf("Thread %d acquired a lock[%p].\n", threadId, mutex);
#endif

	GetLock(&lock, threadId+1);

	// add signature to the rdm
	rdm.addSignature(new SigRaceData(threadId, *vectorClock, *readFilter, *writeFilter));
	fprintf(out, "--- MUTEX LOCK ---\n");

	// increment timestamp
	(*vectorClock)[threadId]++;

	// get the signalling thread and update my timestamp
	SignalThreadIterator itr = signalledThreadMap->find(CONVERT(long, mutex));
	if(itr != signalledThreadMap->end())
	{
		SignalThreadInfo signalThreadInfo = itr->second;
		if (signalThreadInfo.threadId != NO_ID) 
		{
			updateVectorClock(threadId, *vectorClock, signalThreadInfo.vectorClock);
		}

		// remove the notify signal
		SignalThreadIterator stiItr = signalledThreadMap->find(CONVERT(long, mutex));
		stiItr->second.threadId = NO_ID;
	}

	// remove myself from the waiting queue
	WaitQueueIterator foundQueueItr = waitQueueMap->find(CONVERT(long, mutex));
	if(foundQueueItr != waitQueueMap->end() && mutexWaitList)
	{
		mutexWaitList = foundQueueItr->second;
		mutexWaitList->remove(threadId);
	}
	printSignatures();

	ReleaseLock(&lock);

	readFilter->clear();
	writeFilter->clear();
}

VOID BeforeUnlock (pthread_mutex_t * mutex, THREADID threadId)
{
	if(CONVERT(long, mutex) > MUTEX_POINTER_LIMIT)
		return;

	FILE* out   = static_cast<FILE*>(PIN_GetThreadData(tlsKey, threadId));
	VectorClock* vectorClock = static_cast<VectorClock*>(PIN_GetThreadData(vectorClockKey, threadId));
	Bloom* readFilter = static_cast<Bloom*>(PIN_GetThreadData(tlsReadSignatureKey, threadId));
	Bloom* writeFilter = static_cast<Bloom*>(PIN_GetThreadData(tlsWriteSignatureKey, threadId));

#ifdef DEBUG_MODE
		printf("Thread %d released a lock[%p].\n", threadId, mutex);
#endif

	GetLock(&lock, threadId+1);

	// add current signature to the rdm
	rdm.addSignature(new SigRaceData(threadId, *vectorClock, *readFilter, *writeFilter));
	fprintf(out, "--- MUTEX UNLOCK ---\n");

	// increment timestamp
	(*vectorClock)[threadId]++;
	// update the signalled map with my vector clock
	(*signalledThreadMap)[CONVERT(long, mutex)].update(threadId, *vectorClock);

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

#endif
