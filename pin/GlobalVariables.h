#ifndef _GLOBAL_VARIABLES_H
#define _GLOBAL_VARIABLES_H

#include "pin.H"
#include "SigraceModules.h"

#define CONVERT(type, data_ptr) ((type)((void*)data_ptr))
#define GET_ADDR(data_ptr) CONVERT(long, data_ptr)
#define MAX_VC_SIZE 32

//#define DEBUG_MODE

// set 1 GB limit to mutex pointer
#define MUTEX_POINTER_LIMIT 0x40000000

// <<< Thread local storage <<<<<<<<<<<<<<<<<<<<<<
extern TLS_KEY tlsKey;

class ThreadLocalStorage
{
public:
	FILE* out;
	VectorClock* vectorClock;
	Bloom* readBloomFilter;
	Bloom* writeBloomFilter;
	ADDRINT lockAddr;
	ADDRINT condVarAddr;
	ADDRINT barrierAddr;
	//pthread_t* createdThread;
	//pthread_t joinedThread;

	ThreadLocalStorage()
	{
		out = NULL;
		vectorClock = NULL;
		readBloomFilter = NULL;
		writeBloomFilter = NULL;
		lockAddr = 0;
		condVarAddr = 0;
		barrierAddr = 0;
		//createdThread = NULL;
		//joinedThread = NULL;
	}

	~ThreadLocalStorage()
	{
		delete vectorClock;
		delete readBloomFilter;
		delete writeBloomFilter;
	}
};
// >>> Thread local storage >>>>>>>>>>>>>>>>>>>>>>

// <<< Global storage <<<<<<<<<<<<<<<<<<<<<<<<<<<<
extern UINT32 globalId;
extern PIN_LOCK lock;
// >>> Global storage >>>>>>>>>>>>>>>>>>>>>>>>>>>>

#endif
