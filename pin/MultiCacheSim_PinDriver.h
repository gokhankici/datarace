#ifndef _MCS_PINDRIVER_H
#define _MCS_PINDRIVER_H

#include "pin.H"

#include <signal.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <dlfcn.h>

#include "MultiCacheSim-dist/MultiCacheSim.h"
#define MAX_NTHREADS 64

extern std::vector<MultiCacheSim *> Caches;
extern MultiCacheSim *ReferenceProtocol;
extern PIN_LOCK mccLock;

extern bool stopOnError;
extern bool printOnError;

extern unsigned long instrumentationStatus[MAX_NTHREADS];
enum MemOpType { MemRead = 0, MemWrite = 1 };

VOID TurnInstrumentationOn(ADDRINT tid);
VOID TurnInstrumentationOff(ADDRINT tid);
VOID instrumentMCCRoutine(RTN rtn, VOID *v);

VOID Read(THREADID threadid, ADDRINT effectiveAddr, ADDRINT stackPtr, const char* imageName, ADDRINT insPtr, UINT32 readSize);
VOID Write(THREADID threadid, ADDRINT effectiveAddr, ADDRINT stackPtr, const char* imageName, ADDRINT insPtr, UINT32 writeSize);

VOID instrumentTrace(TRACE trace, VOID *v);
BOOL segvHandler(THREADID threadid,INT32 sig,CONTEXT *ctx,BOOL hasHndlr,const EXCEPTION_INFO *pExceptInfo, VOID*v);
BOOL termHandler(THREADID threadid,INT32 sig,CONTEXT *ctx,BOOL hasHndlr,const EXCEPTION_INFO *pExceptInfo, VOID*v);

#endif
