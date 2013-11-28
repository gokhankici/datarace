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

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "lock_mt.out",
		"specify output file name");

KNOB<bool> KnobStopOnError(KNOB_MODE_WRITEONCE, "pintool", "stopOnProtoBug",
		"false",
		"Stop the Simulation when a deviation is detected between the test protocol and the reference"); //default cache is verbose

KNOB<bool> KnobPrintOnError(KNOB_MODE_WRITEONCE, "pintool", "printOnProtoBug",
		"false",
		"Print a debugging message when a deviation is detected between the test protocol and the reference"); //default cache is verbose

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

KNOB<string> KnobReference(KNOB_MODE_WRITEONCE, "pintool", "reference",
		"/home/gokhankici/pin-2.12-55942-gcc.4.4.7-linux/source/tools/datarace/pin/MultiCacheSim-dist/MSI_SMPCache.so",
		"Reference Protocol that is compared to test Protocols for Correctness");

// <<< Thread local storage <<<<<<<<<<<<<<<<<<<<<<
TLS_KEY tlsKey;
// >>> Thread local storage >>>>>>>>>>>>>>>>>>>>>>

// <<< Global storage <<<<<<<<<<<<<<<<<<<<<<<<<<<<
UINT32 globalId = 0;
PIN_LOCK lock;

//WaitQueueMap* waitQueueMap;
UnlockThreadMap* unlockedThreadMap;
NotifyThreadMap* notifiedThreadMap;
PthreadTidMap tidMap;

RaceDetectionModule rdm;
// >>> Global storage >>>>>>>>>>>>>>>>>>>>>>>>>>>>

INT32 usage()
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

int main(INT32 argc, CHAR **argv)
{
	PIN_InitSymbols();
	if (PIN_Init(argc, argv))
	{
		return usage();
	}

	InitLock(&mccLock);
	InitLock(&lock);

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
