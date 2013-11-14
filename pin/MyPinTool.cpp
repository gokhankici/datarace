#include "pin.H"

#include <iostream>
#include <iterator>
#include <stdio.h>
#include <time.h>
#include <list>
#include <map>
#include <assert.h>
#include "MultiCacheSim_PinDriver.h"
#include "Bloom.h"

#define CONVERT(type, data_ptr) ((type)((void*)data_ptr))
#define GET_ADDR(data_ptr) CONVERT(long, data_ptr)
#define NO_ID ((UINT32) 0xFFFFFFFF)
#define MAX_VC_SIZE 32
#define BLOCK_HISTORY_QUEUE_SIZE 10

// set 1 GB limit to mutex pointer
#define MUTEX_POINTER_LIMIT 0x40000000

/* === KNOB DEFINITIONS =================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "lock_mt.out", "specify output file name");

KNOB<bool> KnobStopOnError(KNOB_MODE_WRITEONCE, "pintool", "stopOnProtoBug", "false", "Stop the Simulation when a deviation is detected between the test protocol and the reference");//default cache is verbose 

KNOB<bool> KnobPrintOnError(KNOB_MODE_WRITEONCE, "pintool", "printOnProtoBug", "false", "Print a debugging message when a deviation is detected between the test protocol and the reference");//default cache is verbose 

KNOB<bool> KnobConcise(KNOB_MODE_WRITEONCE, "pintool", "concise", "true", "Print output concisely");//default cache is verbose 

KNOB<unsigned int> KnobCacheSize(KNOB_MODE_WRITEONCE, "pintool", "csize", "65536", "Cache Size");//default cache is 64KB

KNOB<unsigned int> KnobBlockSize(KNOB_MODE_WRITEONCE, "pintool", "bsize", "64", "Block Size");//default block is 64B

KNOB<unsigned int> KnobAssoc(KNOB_MODE_WRITEONCE, "pintool", "assoc", "2", "Associativity");//default associativity is 2-way

KNOB<unsigned int> KnobNumCaches(KNOB_MODE_WRITEONCE, "pintool", "numcaches", "1", "Number of Caches to Simulate");

KNOB<string> KnobProtocol(KNOB_MODE_WRITEONCE, "pintool", "protos", "./MultiCacheSim-dist/MSI_SMPCache.so", "Cache Coherence Protocol Modules To Simulate");

KNOB<string> KnobReference(KNOB_MODE_WRITEONCE, "pintool", "reference", "/home/gokhankici/pin-2.12-55942-gcc.4.4.7-linux/source/tools/datarace/pin/MultiCacheSim-dist/MSI_SMPCache.so", "Reference Protocol that is compared to test Protocols for Correctness");

typedef vector<UINT32> VectorClock;

/*
 * IMPLEMENTATION OF WAITING QUEUE
 */
typedef std::list<UINT32> WaitQueue;
typedef std::map< long, WaitQueue* > WaitQueueMap;
typedef WaitQueueMap::iterator WaitQueueIterator;

class SignalThreadInfo 
{
	public:
		UINT32 threadId;
		VectorClock vectorClock;

		SignalThreadInfo() : threadId(NO_ID) {}

		SignalThreadInfo (UINT32 threadId, const VectorClock& vc) :
			threadId(threadId), vectorClock(vc) {}

		SignalThreadInfo& operator= (const SignalThreadInfo& other)
		{
			SignalThreadInfo temp (other);
			threadId = other.threadId;
			vectorClock = other.vectorClock;

			return *this;
		}

		void update(UINT32 tid, const VectorClock& vc)
		{
			threadId    = tid;
			vectorClock = vc;
		}
};
typedef std::map< long, SignalThreadInfo > SignalThreadMap;
typedef SignalThreadMap::iterator SignalThreadIterator;

/*
 * IMPLEMENTATION OF THREAD SYNCHRONIZATION SIGNATURE
 */
class SigRaceData
{
	public:
		SigRaceData(int threadId, VectorClock& ts, Bloom& r, Bloom& w)
			: threadId(threadId), ts(ts), r(r), w(w) {}

		bool operator<(const SigRaceData& rhs )
		{
			if (threadId == rhs.threadId) 
			{
				return ts[threadId] < rhs.ts[rhs.threadId];
			}

			UINT32 l_l = ts[threadId];
			UINT32 l_r = ts[rhs.threadId];
			UINT32 r_r = rhs.ts[rhs.threadId];
			UINT32 r_l = rhs.ts[threadId];

			// both threads' values are lower in the lhs thread
			return l_l < r_l && l_r < r_r;
		}

		UINT32 threadId;
		VectorClock ts;
		Bloom r;
		Bloom w;
};

/*
 * IMPLEMENTATION OF RACE DETECTION MODULE
 */
typedef std::list<SigRaceData*> BlockHistoryQueue;
class RaceDetectionModule
{
	public:
		RaceDetectionModule() : threadCount(0) { }
		~RaceDetectionModule()
		{
			while(!blockHistoryQueues.empty())
			{
				BlockHistoryQueue* queue = blockHistoryQueues.back();
				blockHistoryQueues.pop_back();
				while(!queue->empty()) 
				{
					delete queue->front();
					queue->pop_front();
				}
				delete queue;
			}
		}

		void addSignature(SigRaceData* sigRaceData)
		{
			// add it to the queue
			BlockHistoryQueue* queue = blockHistoryQueues[sigRaceData->threadId];
			queue->push_front(sigRaceData);

			if (queue->size() > BLOCK_HISTORY_QUEUE_SIZE) 
			{
				delete queue->back();
				queue->pop_back();
			}

			// check it with other threads' values
			for (UINT32 queueId = 0; queueId < blockHistoryQueues.size(); queueId++) 
			{
				if(queueId == sigRaceData->threadId)
				{
					continue;
				}
				queue = blockHistoryQueues[queueId];
				BlockHistoryQueue::iterator itr;
				SigRaceData* other;
				for (itr = queue->begin(); itr != queue->end(); itr++)
				{
					other = *itr;
					// rest is already HB this one
					if (*other < *sigRaceData)
					{
						break;
					}

					if (sigRaceData->r.hasInCommon(other->w)) 
					{
						printf("THERE MAY BE A DATA RACE r-w BETWEEN THREAD-%d & THREAD-%d !!!\n", sigRaceData->threadId, other->threadId);
						goto OUTER_FOR;
					}
					if (sigRaceData->w.hasInCommon(other->r)) 
					{
						printf("THERE MAY BE A DATA RACE w-r BETWEEN THREAD-%d & THREAD-%d !!!\n", sigRaceData->threadId, other->threadId);
						goto OUTER_FOR;
					}
					if (sigRaceData->w.hasInCommon(other->w)) 
					{
						printf("THERE MAY BE A DATA RACE w-w BETWEEN THREAD-%d & THREAD-%d !!!\n", sigRaceData->threadId, other->threadId);
						goto OUTER_FOR;
					}
				}
			}
			OUTER_FOR:
			return;
		}

		void addProcessor()
		{
			threadCount++;
			blockHistoryQueues.push_back(new BlockHistoryQueue);
		}

	private:
		std::vector< BlockHistoryQueue* > blockHistoryQueues;
		int threadCount;
};

RaceDetectionModule rdm;

// <<< Thread local storage <<<<<<<<<<<<<<<<<<<<<<
TLS_KEY tlsKey;
TLS_KEY mutexPtrKey;

TLS_KEY vectorClockKey;
TLS_KEY tlsWriteSignatureKey;
TLS_KEY tlsReadSignatureKey;
// >>> Thread local storage >>>>>>>>>>>>>>>>>>>>>>

// <<< Global storage <<<<<<<<<<<<<<<<<<<<<<<<<<<<
UINT32 globalId=0;
PIN_LOCK lock;

WaitQueueMap* waitQueueMap;
SignalThreadMap* signalledThreadMap;
// >>> Global storage >>>>>>>>>>>>>>>>>>>>>>>>>>>>

//static void writeInHex(const unsigned char* data, int len)
//{
	//for (int i = 0; i < len; i++) 
	//{
		//printf("%02X", data[i]);
	//}
//}

static void printSignatures()
{
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

	printf("Thread %d acquired a lock[%p].\n", threadId, mutex);

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
			//printf("Thread %d happens before %d due to lock %p\n", signalThreadInfo.threadId, threadId, mutex);
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

	//WaitQueue* mutexWaitList = NULL;
	FILE* out   = static_cast<FILE*>(PIN_GetThreadData(tlsKey, threadId));
	VectorClock* vectorClock = static_cast<VectorClock*>(PIN_GetThreadData(vectorClockKey, threadId));
	Bloom* readFilter = static_cast<Bloom*>(PIN_GetThreadData(tlsReadSignatureKey, threadId));
	Bloom* writeFilter = static_cast<Bloom*>(PIN_GetThreadData(tlsWriteSignatureKey, threadId));

	printf("Thread %d released a lock[%p].\n", threadId, mutex);

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

INT32 usage ()
{
    cerr << "An attempt to create SigRace";
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}

VOID Fini(INT32 code, VOID *v)
{
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main (INT32 argc, CHAR **argv)
{
	PIN_InitSymbols();
	if( PIN_Init(argc,argv) ) 
	{
		return usage();
	}

	InitLock(&mccLock);
	InitLock(&lock);

	waitQueueMap = new WaitQueueMap;
	signalledThreadMap = new SignalThreadMap;

	tlsKey         = PIN_CreateThreadDataKey(0);
	mutexPtrKey    = PIN_CreateThreadDataKey(0);
	vectorClockKey = PIN_CreateThreadDataKey(0);
	tlsReadSignatureKey = PIN_CreateThreadDataKey(0);
	tlsWriteSignatureKey = PIN_CreateThreadDataKey(0);

	for(int i = 0; i < MAX_NTHREADS; i++)
	{
		instrumentationStatus[i] = true;
	}

	unsigned long csize = KnobCacheSize.Value();
	unsigned long bsize = KnobBlockSize.Value();
	unsigned long assoc = KnobAssoc.Value();
	unsigned long num = KnobNumCaches.Value();

	const char *pstr = KnobProtocol.Value().c_str();
	char *ct = strtok((char *)pstr,",");
	while(ct != NULL)
	{
		void *chand = dlopen( ct, RTLD_LAZY | RTLD_LOCAL );
		if( chand == NULL )
		{
			fprintf(stderr,"Couldn't Load %s\n", argv[1]);
			fprintf(stderr,"dlerror: %s\n", dlerror());
			exit(1);
		}

		CacheFactory cfac = (CacheFactory)dlsym(chand, "Create");

		if( chand == NULL )
		{
			fprintf(stderr,"Couldn't get the Create function\n");
			fprintf(stderr,"dlerror: %s\n", dlerror());
			exit(1);
		}

		MultiCacheSim *c = new MultiCacheSim(stdout, csize, assoc, bsize, cfac);
		for(unsigned int i = 0; i < num; i++)
		{
			c->createNewCache();
		} 
		fprintf(stderr,"Loaded Protocol Plugin %s\n",ct);
		Caches.push_back(c);

		ct = strtok(NULL,","); 

	}

	void *chand = dlopen( KnobReference.Value().c_str(), RTLD_LAZY | RTLD_LOCAL );
	if( chand == NULL )
	{
		fprintf(stderr,"Couldn't Load Reference: %s\n", argv[1]);
		fprintf(stderr,"dlerror: %s\n", dlerror());
		exit(1);
	}

	CacheFactory cfac = (CacheFactory)dlsym(chand, "Create");

	if( chand == NULL )
	{
		fprintf(stderr,"Couldn't get the Create function\n");
		fprintf(stderr,"dlerror: %s\n", dlerror());
		exit(1);
	}

	ReferenceProtocol = 
		new MultiCacheSim(stdout, csize, assoc, bsize, cfac);

	for(unsigned int i = 0; i < num; i++)
	{
		ReferenceProtocol->createNewCache();
	} 
	fprintf(stderr,"Using Reference Implementation %s\n",KnobReference.Value().c_str());

	stopOnError = KnobStopOnError.Value();
	printOnError = KnobPrintOnError.Value();

	// Register ImageLoad to be called when each image is loaded.
	IMG_AddInstrumentFunction(ImageLoad, 0);
	TRACE_AddInstrumentFunction(instrumentTrace, 0);

	// Register Analysis routines to be called when a thread begins/ends
	PIN_AddThreadStartFunction(ThreadStart, 0);
	PIN_AddThreadFiniFunction(ThreadFini, 0);

	PIN_AddFiniFunction(Fini, 0);

	// Never returns
	PIN_StartProgram();

	return 0;
}
