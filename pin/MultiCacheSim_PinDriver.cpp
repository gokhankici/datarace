#include "pin.H"

#include <signal.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <dlfcn.h>

#include "MultiCacheSim_PinDriver.h"

std::vector<MultiCacheSim *> Caches;
MultiCacheSim *ReferenceProtocol;
PIN_LOCK mccLock;

bool stopOnError = false;
bool printOnError = false;

unsigned long instrumentationStatus[MAX_NTHREADS];

VOID TurnInstrumentationOn(ADDRINT tid)
{
	instrumentationStatus[PIN_ThreadId()] = true; 
}

VOID TurnInstrumentationOff(ADDRINT tid)
{
	instrumentationStatus[PIN_ThreadId()] = false; 
}

void Read(THREADID tid, ADDRINT addr, ADDRINT inst)
{
	GetLock(&mccLock, 1);
	ReferenceProtocol->readLine(tid,inst,addr);
	std::vector<MultiCacheSim *>::iterator i,e;
	for(i = Caches.begin(), e = Caches.end(); i != e; i++)
	{
		(*i)->readLine(tid,inst,addr);
		if(stopOnError || printOnError)
		{
			if( ReferenceProtocol->getStateAsInt(tid,addr) != (*i)->getStateAsInt(tid,addr))
			{
				if(printOnError)
				{
					fprintf(stderr,"[MCS-Read] State of Protocol %s did not match the reference\nShould have been %d but it was %d\n", (*i)->Identify(), ReferenceProtocol->getStateAsInt(tid,addr), (*i)->getStateAsInt(tid,addr));
				}

				if(stopOnError)
				{
					exit(1);
				}
			}
		}
	}

	ReleaseLock(&mccLock);
}

void Write(THREADID tid, ADDRINT addr, ADDRINT inst)
{
	GetLock(&mccLock, 1);
	ReferenceProtocol->writeLine(tid,inst,addr);
	std::vector<MultiCacheSim *>::iterator i,e;

	for(i = Caches.begin(), e = Caches.end(); i != e; i++)
	{
		(*i)->writeLine(tid,inst,addr);

		if(stopOnError || printOnError)
		{
			if( ReferenceProtocol->getStateAsInt(tid,addr) != (*i)->getStateAsInt(tid,addr))
			{
				if(printOnError)
				{
					fprintf(stderr,"[MCS-Write] State of Protocol %s did not match the reference\nShould have been %d but it was %d\n", (*i)->Identify(), ReferenceProtocol->getStateAsInt(tid,addr), (*i)->getStateAsInt(tid,addr));
				}

				if(stopOnError)
				{
					exit(1);
				}
			}
		}
	}
	ReleaseLock(&mccLock);
}

VOID instrumentTrace(TRACE trace, VOID *v)
{
	for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) 
	{
		for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) 
		{  
			if(INS_IsMemoryRead(ins)) 
			{
				INS_InsertCall(ins, 
						IPOINT_BEFORE, 
						(AFUNPTR)Read, 
						IARG_THREAD_ID,
						IARG_MEMORYREAD_EA,
						IARG_INST_PTR,
						IARG_END);
			} 
			else if(INS_IsMemoryWrite(ins)) 
			{
				INS_InsertCall(ins, 
						IPOINT_BEFORE, 
						(AFUNPTR)Write, 
						IARG_THREAD_ID,//thread id
						IARG_MEMORYWRITE_EA,//address being accessed
						IARG_INST_PTR,//instruction address of write
						IARG_END);
			}
		}
	}
}

BOOL segvHandler(THREADID threadid,INT32 sig,CONTEXT *ctx,BOOL hasHndlr,const EXCEPTION_INFO *pExceptInfo, VOID*v)
{
	return TRUE;//let the program's handler run too
}

BOOL termHandler(THREADID threadid,INT32 sig,CONTEXT *ctx,BOOL hasHndlr,const EXCEPTION_INFO *pExceptInfo, VOID*v)
{
	return TRUE;//let the program's handler run too
}
