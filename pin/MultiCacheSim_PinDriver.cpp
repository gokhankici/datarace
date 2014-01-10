#include "pin.H"

#include <signal.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <dlfcn.h>
#include <assert.h>

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
	if (static_cast<UINT64>(abs(stackPtr - effectiveAddr)) >
	        STACK_PTR_ERROR)
	{
		return true;
	}

	return false;
}

VOID addToReadFilter(THREADID tid, ADDRINT addr, ADDRINT stackPtr)
{
	if (isMemoryGlobal(addr, stackPtr))
	{
		ThreadLocalStorage* tls =
		    static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
		Bloom* readSig = tls->readBloomFilter;

#ifdef WRITE_ADDRESSES_TO_LOG_FILE

		fprintf(tls->out, "R : %lX\n", addr);
#endif

		GetLock(&rdmLock, tid + 1);
		readSig->add(BLOOM_ADDR(addr));
		ReleaseLock(&rdmLock);
	}
}

//void Read(THREADID tid, ADDRINT addr, ADDRINT inst)
void Read(THREADID tid, ADDRINT addr, ADDRINT stackPtr, const char* imageName,
          ADDRINT inst, UINT32 readSize)
{
	addToReadFilter(tid, addr, stackPtr);

	/* addition of MultiCacheSim coherency protocols */
	/*
	 GetLock(&mccLock, 1);
	 ReferenceProtocol->readLine(tid, inst, addr);
	 std::vector<MultiCacheSim *>::iterator i, e;
	 for (i = Caches.begin(), e = Caches.end(); i != e; i++)
	 {
	 (*i)->readLine(tid, inst, addr);
	 if (stopOnError || printOnError)
	 {
	 if (ReferenceProtocol->getStateAsInt(tid, addr)
	 != (*i)->getStateAsInt(tid, addr))
	 {
	 if (printOnError)
	 {
	 fprintf(stderr,
	 "[MCS-Read] State of Protocol %s did not match the reference\nShould have been %d but it was %d\n",
	 (*i)->Identify(),
	 ReferenceProtocol->getStateAsInt(tid, addr),
	 (*i)->getStateAsInt(tid, addr));
	 }

	 if (stopOnError)
	 {
	 exit(1);
	 }
	 }
	 }
	 }

	 ReleaseLock(&mccLock);
	 */
}

VOID addToWriteFilter(THREADID tid, ADDRINT addr, ADDRINT stackPtr)
{
	if (isMemoryGlobal(addr, stackPtr))
	{
		ThreadLocalStorage* tls =
		    static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
		Bloom* writeSig = tls->writeBloomFilter;

#ifdef WRITE_ADDRESSES_TO_LOG_FILE

		fprintf(tls->out, "W : %lX\n", addr);
#endif

		GetLock(&rdmLock, tid + 1);
		writeSig->add(BLOOM_ADDR(addr));
		ReleaseLock(&rdmLock);
	}
}

//void Write(THREADID tid, ADDRINT addr, ADDRINT inst)
void Write(THREADID tid, ADDRINT addr, ADDRINT stackPtr, const char* imageName,
           ADDRINT inst, UINT32 writeSize)
{
	addToWriteFilter(tid, addr, stackPtr);

	/*
	 GetLock(&mccLock, 1);
	 ReferenceProtocol->writeLine(tid, inst, addr);
	 std::vector<MultiCacheSim *>::iterator i, e;

	 for (i = Caches.begin(), e = Caches.end(); i != e; i++)
	 {
	 (*i)->writeLine(tid, inst, addr);

	 if (stopOnError || printOnError)
	 {
	 if (ReferenceProtocol->getStateAsInt(tid, addr)
	 != (*i)->getStateAsInt(tid, addr))
	 {
	 if (printOnError)
	 {
	 fprintf(stderr,
	 "[MCS-Write] State of Protocol %s did not match the reference\nShould have been %d but it was %d\n",
	 (*i)->Identify(),
	 ReferenceProtocol->getStateAsInt(tid, addr),
	 (*i)->getStateAsInt(tid, addr));
	 }

	 if (stopOnError)
	 {
	 exit(1);
	 }
	 }
	 }
	 }
	 ReleaseLock(&mccLock);
	 */
}

void processMemoryWriteInstruction(INS ins, const char* imageName)
{
	UINT32 memoryOperandCount = INS_MemoryOperandCount(ins);
	for (UINT32 i = 0; i < memoryOperandCount; ++i)
	{
		if (INS_MemoryOperandIsWritten(ins, i) && INS_OperandIsMemory(ins, i))
		{
			INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR) Write,
			                         IARG_THREAD_ID, IARG_MEMORYOP_EA, i, IARG_REG_VALUE,
			                         REG_STACK_PTR, //pass current stack ptr
			                         IARG_PTR, imageName, IARG_INST_PTR, IARG_MEMORYWRITE_SIZE,
			                         IARG_CALL_ORDER, CALL_ORDER_FIRST + 30, IARG_END);
		}
	}
}

void processMemoryReadInstruction(INS ins, const char* imageName)
{
	UINT32 memoryOperandCount = INS_MemoryOperandCount(ins);
	for (UINT32 i = 0; i < memoryOperandCount; ++i)
	{
		if (INS_MemoryOperandIsRead(ins, i))
		{
			INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR) Read,
			                         IARG_THREAD_ID, IARG_MEMORYOP_EA, i, IARG_REG_VALUE,
			                         REG_STACK_PTR, //pass current stack ptr
			                         IARG_PTR, imageName, IARG_INST_PTR, IARG_MEMORYWRITE_SIZE,
			                         IARG_CALL_ORDER, CALL_ORDER_FIRST + 30, IARG_END);
		}
	}
}

VOID instrumentTrace(TRACE trace, VOID *v)
{
	IMG img = IMG_FindByAddress(TRACE_Address(trace));

	if (!IMG_Valid(img) || !IMG_IsMainExecutable(img))
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

BOOL segvHandler(THREADID threadid, INT32 sig, CONTEXT *ctx, BOOL hasHndlr,
                 const EXCEPTION_INFO *pExceptInfo, VOID*v)
{
	return TRUE; //let the program's handler run too
}

BOOL termHandler(THREADID threadid, INT32 sig, CONTEXT *ctx, BOOL hasHndlr,
                 const EXCEPTION_INFO *pExceptInfo, VOID*v)
{
	return TRUE; //let the program's handler run too
}

/* ############################################################# */
/* ############################################################# */
/* ##################   MALLOC / FREE    ####################### */
/* ############################################################# */
/* ############################################################# */

static ThreadLocalStorage* getTLS(THREADID tid)
{
	ThreadLocalStorage* tls =
	    static_cast<ThreadLocalStorage*>(PIN_GetThreadData(tlsKey, tid));
	return tls;
}

/*
 * Finds the first (smallest starting address) memory area that overlaps
 * with the given area
 */
#pragma INLINE
static MemorySetItr smallestOverlappingMemoryArea(const MemoryArea& area)
{
	MemorySetItr itr = memorySet.lower_bound(area);
	return itr;
}

#pragma INLINE
static const MemoryArea& findMemoryArea(ADDRINT from)
{
	MemoryArea area(0, from, 0);
	MemorySetItr itr = memorySet.find(area);
	return *itr;
}

//#pragma INLINE
//static bool hasOverlappingArea(MemoryArea& area)
//{
//	MemorySetItr itr = memorySet.lower_bound(area);
//	return itr != memorySet.end() && ((MemoryArea) *itr).overlaps(area);
//}

static VOID moveMemoryAddresses(ADDRINT startOfOldPlace,
                                ADDRINT startOfNewPlace, UINT32 size, THREADID tid)
{
	/*
	 ADDRINT maxSize = (startOfOldPlace + size);
	 ADDRINT newMemAddrTmp;
	 variablesHashMap* tmpHashMap;
	 MemoryAddr* tmpMemAddr = NULL;
	 MemoryAddr* newTmpMemAddrObj = NULL;

	 ADDRINT difference = 0;
	 for (ADDRINT currentAddress = startOfOldPlace; currentAddress <= maxSize;
	 ++currentAddress)
	 {
	 GetLock(
	 &variablePinLocks[(currentAddress >> 3)
	 & allMemoryLocations.bitMask], tid);
	 tmpHashMap = allMemoryLocations.getVariableHashMap(currentAddress);
	 if (tmpHashMap == NULL || (*tmpHashMap).count(currentAddress) == 0)
	 {
	 ReleaseLock (&variablePinLocks[(currentAddress >> 3)
	 & allMemoryLocations.bitMask]);
	 continue;
	 }
	 tmpMemAddr = (*tmpHashMap)[currentAddress];
	 difference = currentAddress - startOfOldPlace;
	 ReleaseLock (&variablePinLocks[(currentAddress >> 3)
	 & allMemoryLocations.bitMask]);

	 newMemAddrTmp = startOfNewPlace + difference;

	 GetLock(
	 &variablePinLocks[(newMemAddrTmp >> 3)
	 & allMemoryLocations.bitMask], tid);
	 tmpHashMap = allMemoryLocations.getVariableHashMap(newMemAddrTmp);
	 if (tmpHashMap == NULL)
	 {
	 ReleaseLock(
	 &variablePinLocks[(newMemAddrTmp >> 3)
	 & allMemoryLocations.bitMask]);
	 continue;
	 }
	 //cout << "moved:" << currentAddress << "  to:" << newMemAddrTmp << endl;
	 newTmpMemAddrObj = new MemoryAddr(newMemAddrTmp, true); //yeni mem adresi olustur
	 (*tmpHashMap)[newMemAddrTmp] = newTmpMemAddrObj;
	 newTmpMemAddrObj->readerVectorClock = tmpMemAddr->readerVectorClock; //vector clocklari al
	 newTmpMemAddrObj->writerVectorClock = tmpMemAddr->writerVectorClock;
	 tmpMemAddr->readerVectorClock = NULL; //eskinin vectorClocklari null'a
	 tmpMemAddr->writerVectorClock = NULL;
	 ReleaseLock(
	 &variablePinLocks[(newMemAddrTmp >> 3)
	 & allMemoryLocations.bitMask]);

	 GetLock(
	 &variablePinLocks[(currentAddress >> 3)
	 & allMemoryLocations.bitMask], tid);
	 tmpHashMap = allMemoryLocations.getVariableHashMap(currentAddress);
	 tmpHashMap->erase(currentAddress);
	 delete tmpMemAddr;
	 ReleaseLock(
	 &variablePinLocks[(currentAddress >> 3)
	 & allMemoryLocations.bitMask]);
	 }
	 */
}

static VOID freeMemoryAddress(ADDRINT memoryAddrFreeStarts,
                              ADDRINT maxMemoryAddrToBeFreed, THREADID threadid)
{
	/*
	 variablesHashMap* tmpHashMap;
	 for (ADDRINT currentAddress = memoryAddrFreeStarts;
	 currentAddress <= maxMemoryAddrToBeFreed; ++currentAddress)
	 {
	 GetLock(
	 &variablePinLocks[(currentAddress >> 3)
	 & allMemoryLocations.bitMask], threadid);
	 tmpHashMap = allMemoryLocations.getVariableHashMap(currentAddress);

	 if (tmpHashMap == NULL || (*tmpHashMap).count(currentAddress) == 0)
	 {
	 ReleaseLock (&variablePinLocks[(currentAddress >> 3)
	 & allMemoryLocations.bitMask]);
	 continue;
	 }

	 delete (*tmpHashMap)[currentAddress]->writerVectorClock;
	 delete (*tmpHashMap)[currentAddress]->readerVectorClock;

	 (*tmpHashMap).erase(currentAddress);
	 ReleaseLock (&variablePinLocks[(currentAddress >> 3)
	 & allMemoryLocations.bitMask]);
	 }
	 */
}

VOID ReallocEnter(CHAR * name, ADDRINT previousAddress, ADDRINT newSize,
                  THREADID tid)
{
	ThreadLocalStorage* tls = getTLS(tid);
	tls->nextReallocAddr = previousAddress;
	tls->nextReallocSize = newSize;
}

VOID ReallocAfter(ADDRINT mallocStartAddr, THREADID tid)
{
	ThreadLocalStorage* tls = getTLS(tid);
	ADDRINT previousAddress = tls->nextReallocAddr;
	ADDRINT newSize = tls->nextReallocSize;

	//if previous address is NULL, this is the same as malloc
	if (previousAddress == 0)
	{
		tls->nextMallocSize = tls->nextReallocSize;
		tls->nextReallocAddr = 0;
		tls->nextReallocSize = 0;
		MallocAfter(mallocStartAddr, tid);
		return;
	}

	const MemoryArea& prevArea = findMemoryArea(mallocStartAddr);
	MemoryArea newArea(tid, mallocStartAddr, mallocStartAddr + newSize);

	ADDRINT prevSize = prevArea.to - prevArea.from;

	// Realloc gives the same address as previos realloc(or malloc,calloc)
	if (prevArea.from == mallocStartAddr)
	{
		//eger yeni yer daha kucuk ise, eskiden bizim olan aradaki yerleri free edelim
		if (newSize < prevSize)
		{
			freeMemoryAddress(mallocStartAddr + newSize,
			                  mallocStartAddr + prevSize, tid);
		}
	}
	else
	{
		if (newSize < prevSize)
		{
			moveMemoryAddresses(previousAddress, mallocStartAddr, newSize, tid);
			freeMemoryAddress(previousAddress + newSize,
			                  previousAddress + prevSize, tid);
		}
		else
		{
			moveMemoryAddresses(previousAddress, mallocStartAddr, prevSize,
			                    tid);
			freeMemoryAddress(previousAddress, previousAddress + prevSize, tid);
		}
	}

	GetLock(&memorySetLock, tid);
	memorySet.insert(newArea);
	memorySet.erase(prevArea);
	ReleaseLock(&memorySetLock);
}

VOID CallocEnter(CHAR * name, ADDRINT nElements, ADDRINT sizeOfEachElement,
                 THREADID tid)
{
	ThreadLocalStorage* tls = getTLS(tid);
	tls->nextMallocSize = nElements * sizeOfEachElement;
}

VOID MallocEnter(CHAR * name, ADDRINT size, THREADID tid)
{
	ThreadLocalStorage* tls = getTLS(tid);
	tls->nextMallocSize = size;
}

VOID MallocAfter(ADDRINT mallocStartAddr, THREADID tid)
{
	ThreadLocalStorage* tls = getTLS(tid);
	ADDRINT mallocEndAddr = tls->nextMallocSize + mallocStartAddr;
	tls->nextMallocSize = 0;

	MemoryArea newArea(tid, mallocStartAddr, mallocEndAddr);

	GetLock(&memorySetLock, tid);
	MemorySetItr overlaps = smallestOverlappingMemoryArea(newArea);

	if (overlaps != memorySet.end())
	{
		// has overlapping elements
	}

	ReleaseLock(&memorySetLock);
}

VOID FreeEnter(ADDRINT startAddress, THREADID threadid)
{
	GetLock(&memorySetLock, threadid);
	const MemoryArea area = findMemoryArea(startAddress);
	ADDRINT freeSize = area.size();
	memorySet.erase(area);
	ReleaseLock(&memorySetLock);

	if (freeSize == 0)
		return;

	freeMemoryAddress(startAddress, area.to, threadid);
}
