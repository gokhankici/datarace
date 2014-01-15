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

#define NOT_A_THREADID 0xFFFFFFFF

extern TLS_KEY tlsKey;

typedef map<pthread_t, THREADID> PthreadPinIdMap;
typedef PthreadPinIdMap::iterator PthreadPinIdMapItr;

typedef map<THREADID, VectorClock> ChildVCMap;
typedef ChildVCMap::iterator ChildVCMapItr;

#define EASSERT(expr) assert((expr))
#define EASSERT_LOG(expr, err_msg) \
	assert((expr) || (printf(err_msg) && 0))
#define EASSERT_MSG(expr, err_msg, ...) \
	assert((expr) || (printf(err_msg, __VA_ARGS__) && 0))

class ThreadLocalStorage
{
public:
	FILE* out;
	VectorClock* vectorClock;

	ChildVCMap joinVCMap;

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
		if(out)
			fclose(out);

		if(vectorClock)
			delete vectorClock;

		if(readBloomFilter)
			delete readBloomFilter;

		if(writeBloomFilter)
			delete writeBloomFilter;
	}
};

#endif
