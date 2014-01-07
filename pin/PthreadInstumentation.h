#ifndef _PTHREAD_INSTRUMENTATION_H_
#define _PTHREAD_INSTRUMENTATION_H_

#include "pin.H"

#include "MultiCacheSim_PinDriver.h"
#include "SigraceModules.h"
#include "Bloom.h"
#include "MyFlags.h"

extern PIN_LOCK rdmLock;

//extern WaitQueueMap* waitQueueMap;
extern UnlockThreadMap* unlockedThreadMap;
extern NotifyThreadMap* notifiedThreadMap;
extern RaceDetectionModule rdm;

extern PIN_LOCK threadIdMapLock;
extern ThreadIdMap threadIdMap;

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
