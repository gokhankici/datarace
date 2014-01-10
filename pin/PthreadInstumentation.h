#ifndef _PTHREAD_INSTRUMENTATION_H_
#define _PTHREAD_INSTRUMENTATION_H_

#include "pin.H"

#include "MultiCacheSim_PinDriver.h"
#include "SigraceModules.h"
#include "Bloom.h"
#include "MyFlags.h"

#define SLEEP_DURATION   50
#define MAX_WAKEUP_COUNT 100

extern PIN_LOCK rdmLock;

//extern WaitQueueMap* waitQueueMap;
extern UnlockThreadMap* unlockedThreadMap;
extern NotifyThreadMap* notifiedThreadMap;
extern RaceDetectionModule rdm;

extern PIN_LOCK atomicCreate;

extern PIN_LOCK threadIdMapLock;
extern ThreadIdMap threadIdMap;

extern PthreadPinIdMap pthreadPinIdMap;

extern PIN_LOCK barrierLock;
extern BarrierMap barrierWaitMap;

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

// variables to handle the order of thread creation
extern THREADID lastParent;
extern int createdThreadCount;
extern FILE* createFile;

// used to define the point of instrumentation
#define INSTRUMENT_BEFORE 1
#define INSTRUMENT_AFTER  2
#define INSTRUMENT_BOTH   (INSTRUMENT_BEFORE | INSTRUMENT_AFTER)

#define ARGUMENT_LIST_0   IARG_END
#define ARGUMENT_LIST_1   IARG_FUNCARG_ENTRYPOINT_VALUE, 0,\
                          ARGUMENT_LIST_0
#define ARGUMENT_LIST_2   IARG_FUNCARG_ENTRYPOINT_VALUE, 0,\
                          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,\
                          ARGUMENT_LIST_0
#define ARGUMENT_LIST_3   IARG_FUNCARG_ENTRYPOINT_VALUE, 0,\
                          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,\
                          IARG_FUNCARG_ENTRYPOINT_VALUE, 2,\
                          ARGUMENT_LIST_0
#define ARGUMENT_LIST_4   IARG_FUNCARG_ENTRYPOINT_VALUE, 0,\
                          IARG_FUNCARG_ENTRYPOINT_VALUE, 1,\
                          IARG_FUNCARG_ENTRYPOINT_VALUE, 2,\
                          IARG_FUNCARG_ENTRYPOINT_VALUE, 3,\
                          ARGUMENT_LIST_0

#define PTHREAD_CREATE_ARGS IARG_THREAD_ID,\
                            IARG_CONST_CONTEXT,\
	                        IARG_ORIG_FUNCPTR,\
                            ARGUMENT_LIST_4

#define PTHREAD_JOIN_ARGS   IARG_THREAD_ID,\
                            IARG_CONST_CONTEXT,\
	                        IARG_ORIG_FUNCPTR,\
                            ARGUMENT_LIST_2


class MyStartRoutineArgs
{
public:
	void* origArgs;
	pthread_t* childId;

	MyStartRoutineArgs(void* origArgs, pthread_t* childId) :
			origArgs(origArgs), childId(childId)
	{}
}
;


VOID ImageLoad(IMG img, VOID *);

VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v);
VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v);

VOID BeforeLock(THREADID tid, ADDRINT lockAddr);
VOID AfterLock(THREADID tid, int returnValue);
VOID AfterTryLock(THREADID tid, int returnValue);
VOID BeforeUnlock(THREADID tid, ADDRINT lockAddr);

VOID BeforeCondWait(THREADID tid, ADDRINT condVarAddr, ADDRINT lockAddr);
VOID AfterCondWait(THREADID tid, int returnValue);
VOID BeforeCondSignal(THREADID tid, ADDRINT condVarAddr);
VOID BeforeCondBroadcast(THREADID tid, ADDRINT condVarAddr);

VOID BeforeBarrierInit(THREADID tid, ADDRINT barrier, ADDRINT barrierAttr, int size);
VOID BeforeBarrierWait(THREADID tid, ADDRINT barrier);
VOID AfterBarrierWait(THREADID tid, int returnValue);

VOID Fini(INT32 code, VOID *v);

#endif
