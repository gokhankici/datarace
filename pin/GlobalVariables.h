#ifndef _GLOBAL_VARIABLES_H
#define _GLOBAL_VARIABLES_H

#include "pin.H"
#include "SigraceModules.h"
#include "MyFlags.h"
#include <deque>

#define CONVERT(type, data_ptr) ((type)((void*)data_ptr))
#define GET_ADDR(data_ptr) CONVERT(long, data_ptr)

// set 1 GB limit to mutex pointer
#define MUTEX_POINTER_LIMIT 0x40000000

extern TLS_KEY tlsKey;

class ThreadLocalStorage
{
public:
	FILE* out;
	VectorClock* vectorClock;

	deque<VectorClock> createVCList;
	deque<VectorClock> joinVCList;

	Bloom* readBloomFilter;
	Bloom* writeBloomFilter;
	ADDRINT lockAddr;
	ADDRINT condVarAddr;
	ADDRINT barrierAddr;

	ADDRINT nextMallocSize;
	ADDRINT nextReallocAddr;
	ADDRINT nextReallocSize;

	ThreadLocalStorage()
	{
		out = NULL;
		vectorClock = NULL;
		readBloomFilter = NULL;
		writeBloomFilter = NULL;
		lockAddr = 0;
		condVarAddr = 0;
		barrierAddr = 0;

		nextMallocSize = 0;
		nextReallocAddr = 0;
		nextReallocSize = 0;
	}

	~ThreadLocalStorage()
	{
		delete vectorClock;
		delete readBloomFilter;
		delete writeBloomFilter;
	}
};

#endif
