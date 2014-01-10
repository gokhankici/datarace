#include "pin.H"

#include <iostream>
#include <iterator>
#include <stdio.h>
#include <time.h>
#include <list>
#include <map>
#include <assert.h>

#include "GlobalVariables.h"
#include "SigraceModules.h"
#include "PthreadInstumentation.h"
#include "MultiCacheSim_PinDriver.h"
#include "Bloom.h"

/* === KNOB DEFINITIONS =================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o",
		"thread_epochs", "specify output file name");

KNOB<bool> KnobStopOnError(KNOB_MODE_WRITEONCE, "pintool", "stopOnProtoBug",
		"false",
		//default cache is verbose
		"Stop the Simulation when a deviation is detected between the test protocol "
				"and the reference");

KNOB<bool> KnobPrintOnError(KNOB_MODE_WRITEONCE, "pintool", "printOnProtoBug",
		"false",
		"Print a debugging message when a deviation is detected between the test protocol "
				"and the reference"); //default cache is verbose

KNOB<bool> KnobConcise(KNOB_MODE_WRITEONCE, "pintool", "concise", "true",
		"Print output concisely"); //default cache is verbose

KNOB<unsigned int> KnobCacheSize(KNOB_MODE_WRITEONCE, "pintool", "csize",
		"65536", "Cache Size"); //default cache is 64KB

KNOB<unsigned int> KnobBlockSize(KNOB_MODE_WRITEONCE, "pintool", "bsize", "64",
		"Block Size"); //default block is 64B

KNOB<unsigned int> KnobAssoc(KNOB_MODE_WRITEONCE, "pintool", "assoc", "2",
		"Associativity"); //default associativity is 2-way

KNOB<unsigned int> KnobNumCaches(KNOB_MODE_WRITEONCE, "pintool", "numcaches",
		"1", "Number of Caches to Simulate");

KNOB<string> KnobProtocol(KNOB_MODE_WRITEONCE, "pintool", "protos",
		"./MultiCacheSim-dist/MSI_SMPCache.so",
		"Cache Coherence Protocol Modules To Simulate");

KNOB<string> KnobCreateFile(KNOB_MODE_WRITEONCE, "pintool", "createFile",
		"create.txt", "specify create file to order the thread creations");

#ifdef ON_MY_MACHINE
KNOB<string> KnobReference(KNOB_MODE_WRITEONCE, "pintool", "reference",
		"/home/gokhan/Applications/pin-2.12-55942-gcc.4.4.7-linux/source/tools/datarace/"
				"pin/MultiCacheSim-dist/MSI_SMPCache.so",
		"Reference Protocol that is compared to test Protocols for Correctness");
#else
KNOB<string> KnobReference(KNOB_MODE_WRITEONCE, "pintool", "reference",
		"/home/gokhankici/pin-2.12-55942-gcc.4.4.7-linux/source/tools/datarace/"
		"pin/MultiCacheSim-dist/MSI_SMPCache.so",
		"Reference Protocol that is compared to test Protocols for Correctness");
#endif

// <<< Thread local storage <<<<<<<<<<<<<<<<<<<<<<
TLS_KEY tlsKey;

PthreadPinIdMap pthreadPinIdMap;
// >>> Thread local storage >>>>>>>>>>>>>>>>>>>>>>

// <<< Global storage <<<<<<<<<<<<<<<<<<<<<<<<<<<<
PIN_LOCK rdmLock;
PIN_LOCK atomicCreate;
PIN_LOCK barrierLock;
PIN_LOCK threadIdMapLock;
PIN_LOCK fileLock;
PIN_LOCK memorySetLock;

THREADID lastCreatedThread = NOT_A_THREADID;
ThreadIdMap threadIdMap;

//WaitQueueMap* waitQueueMap;
UnlockThreadMap* unlockedThreadMap;
NotifyThreadMap* notifiedThreadMap;
BarrierMap barrierWaitMap;

RaceDetectionModule rdm;
MemorySet memorySet;

// record-n-replay
UINT32 globalEventId = -1;
PIN_LOCK recordLock;
FILE* recordFile = NULL;
FILE* raceInfoFile = NULL;

ThreadCreateOrder threadCreateOrder;
FILE* createFile;

// >>> Global storage >>>>>>>>>>>>>>>>>>>>>>>>>>>>

INT32 usage()
{
	cerr << "An attempt to create SigRace";
	cerr << KNOB_BASE::StringKnobSummary();
	cerr << endl;
	return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(INT32 argc, CHAR **argv)
{
	PIN_InitSymbols();
	if (PIN_Init(argc, argv))
	{
		return usage();
	}

	InitLock(&fileLock);
	InitLock(&mccLock);
	InitLock(&rdmLock);
	InitLock(&threadIdMapLock);
	InitLock(&barrierLock);
	InitLock(&memorySetLock);
	InitLock(&recordLock);
	InitLock(&atomicCreate);

	recordFile = fopen("record.txt", "w");
	raceInfoFile = fopen("race_info.txt", "w");
	createFile = fopen(KnobCreateFile.Value().c_str(), "w");

	//waitQueueMap = new WaitQueueMap;
	unlockedThreadMap = new UnlockThreadMap;
	notifiedThreadMap = new NotifyThreadMap;
	tlsKey = PIN_CreateThreadDataKey(0);

	for (int i = 0; i < MAX_NTHREADS; i++)
	{
		instrumentationStatus[i] = true;
	}

	unsigned long csize = KnobCacheSize.Value();
	unsigned long bsize = KnobBlockSize.Value();
	unsigned long assoc = KnobAssoc.Value();
	unsigned long num = KnobNumCaches.Value();

	const char *pstr = KnobProtocol.Value().c_str();
	char *ct = strtok((char *) pstr, ",");
	while (ct != NULL)
	{
		void *chand = dlopen(ct, RTLD_LAZY | RTLD_LOCAL);
		if (chand == NULL)
		{
			fprintf(stderr, "Couldn't Load %s\n", argv[1]);
			fprintf(stderr, "dlerror: %s\n", dlerror());
			exit(1);
		}

		CacheFactory cfac = (CacheFactory) dlsym(chand, "Create");

		if (chand == NULL)
		{
			fprintf(stderr, "Couldn't get the Create function\n");
			fprintf(stderr, "dlerror: %s\n", dlerror());
			exit(1);
		}

		MultiCacheSim *c = new MultiCacheSim(stdout, csize, assoc, bsize, cfac);
		for (unsigned int i = 0; i < num; i++)
		{
			c->createNewCache();
		}
		fprintf(stderr, "Loaded Protocol Plugin %s\n", ct);
		Caches.push_back(c);

		ct = strtok(NULL, ",");

	}

	void *chand = dlopen(KnobReference.Value().c_str(), RTLD_LAZY | RTLD_LOCAL);
	if (chand == NULL)
	{
		fprintf(stderr, "Couldn't Load Reference: %s\n", argv[1]);
		fprintf(stderr, "dlerror: %s\n", dlerror());
		exit(1);
	}

	CacheFactory cfac = (CacheFactory) dlsym(chand, "Create");

	if (chand == NULL)
	{
		fprintf(stderr, "Couldn't get the Create function\n");
		fprintf(stderr, "dlerror: %s\n", dlerror());
		exit(1);
	}

	ReferenceProtocol = new MultiCacheSim(stdout, csize, assoc, bsize, cfac);

	for (unsigned int i = 0; i < num; i++)
	{
		ReferenceProtocol->createNewCache();
	}
	fprintf(stderr, "Using Reference Implementation %s\n",
			KnobReference.Value().c_str());

	stopOnError = KnobStopOnError.Value();
	printOnError = KnobPrintOnError.Value();

	// Register ImageLoad to be called when each image is loaded.
	IMG_AddInstrumentFunction(ImageLoad, NULL);
	TRACE_AddInstrumentFunction(instrumentTrace, NULL);

	// Register Analysis routines to be called when a thread begins/ends
	PIN_AddThreadStartFunction(ThreadStart, 0);
	PIN_AddThreadFiniFunction(ThreadFini, 0);

	PIN_AddFiniFunction(Fini, 0);

	// Never returns
	PIN_StartProgram();

	return 0;
}
