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

extern PIN_LOCK threadIdMapLock;
extern ThreadIdMap threadIdMap;

extern PIN_LOCK barrierLock;
extern BarrierQueueMap barrierWaitMap;

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

#endif
