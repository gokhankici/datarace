#include "pin.H"

#include <signal.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <dlfcn.h>

#include "GlobalVariables.h"
#include "MultiCacheSim_PinDriver.h"
#include "Bloom.h"

ADDRINT STACK_PTR_ERROR = 1000;

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

BOOL isMemoryGlobal(ADDRINT effectiveAddr, ADDRINT stackPtr)
{
	//if stack pointer is greater, it is global or in heap --> shared
	if (static_cast<UINT64> (abs(stackPtr - effectiveAddr)) > STACK_PTR_ERROR) 
	{
		return true;
	}

	return false;
}

VOID addToReadFilter(THREADID tid, ADDRINT addr, ADDRINT stackPtr)
{
	if (isMemoryGlobal(addr, stackPtr))
	{
		ThreadLocalStorage* tls = static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
		FILE* out   = tls->out;
		Bloom* readSig = tls->writeBloomFilter;

		fprintf(out , "R : %lX\n", addr);
		readSig->add(BLOOM_ADDR(addr));
	}
}

//void Read(THREADID tid, ADDRINT addr, ADDRINT inst)
void Read(THREADID tid, ADDRINT addr, ADDRINT stackPtr, const char* imageName, ADDRINT inst, UINT32 readSize)
{
	addToReadFilter(tid, addr, stackPtr);

	/* addition of MultiCacheSim coherency protocols */
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

VOID addToWriteFilter(THREADID tid, ADDRINT addr, ADDRINT stackPtr)
{
	if (isMemoryGlobal(addr, stackPtr))
	{
		ThreadLocalStorage* tls = static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
		FILE* out   = tls->out;
		Bloom* writeSig = tls->writeBloomFilter;
		fprintf(out , "W : %lX\n", addr);

		writeSig->add(BLOOM_ADDR(addr));
	}
}

//void Write(THREADID tid, ADDRINT addr, ADDRINT inst)
void Write(THREADID tid, ADDRINT addr, ADDRINT stackPtr, const char* imageName, ADDRINT inst, UINT32 writeSize)
{
	addToWriteFilter(tid, addr, stackPtr);

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

void processMemoryWriteInstruction(INS ins, const char* imageName)
{
	UINT32 memoryOperandCount = INS_MemoryOperandCount(ins);
	for(UINT32 i = 0; i < memoryOperandCount; ++i)
	{
		if (INS_MemoryOperandIsWritten(ins,i) && INS_OperandIsMemory(ins,i))
		{
			INS_InsertPredicatedCall(
					ins, IPOINT_BEFORE, (AFUNPTR)Write,
					IARG_THREAD_ID,
					IARG_MEMORYOP_EA, i,
					IARG_REG_VALUE, REG_STACK_PTR, //pass current stack ptr
					IARG_PTR, imageName,
					IARG_INST_PTR,
					IARG_MEMORYWRITE_SIZE,
					IARG_CALL_ORDER, CALL_ORDER_FIRST + 30,
					IARG_END);
		}
	}
}

void processMemoryReadInstruction(INS ins, const char* imageName)
{
	UINT32 memoryOperandCount = INS_MemoryOperandCount(ins);
	for(UINT32 i = 0; i < memoryOperandCount; ++i)
		if (INS_MemoryOperandIsRead(ins, i))
		{

			INS_InsertPredicatedCall(
					ins, IPOINT_BEFORE, (AFUNPTR)Read,
					IARG_THREAD_ID,
					IARG_MEMORYOP_EA, i,
					IARG_REG_VALUE, REG_STACK_PTR,//pass current stack ptr
					IARG_PTR, imageName,
					IARG_INST_PTR,
					IARG_MEMORYWRITE_SIZE,
					IARG_CALL_ORDER, CALL_ORDER_FIRST + 30,
					IARG_END);
		}
}

VOID instrumentTrace(TRACE trace, VOID *v)
{
	IMG img = IMG_FindByAddress(TRACE_Address(trace));

	if (!IMG_Valid(img) || !IMG_IsMainExecutable(img) )
		return;

	const char* imageName = IMG_Name(img).c_str();

	for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) 
	{
		for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) 
		{  
			processMemoryWriteInstruction(ins, imageName);
			processMemoryReadInstruction(ins, imageName);
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
