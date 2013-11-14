#ifndef _GLOBAL_VARIABLES_H
#define _GLOBAL_VARIABLES_H

#include "pin.H"

#define CONVERT(type, data_ptr) ((type)((void*)data_ptr))
#define GET_ADDR(data_ptr) CONVERT(long, data_ptr)
#define MAX_VC_SIZE 32

//#define DEBUG_MODE

// set 1 GB limit to mutex pointer
#define MUTEX_POINTER_LIMIT 0x40000000

// <<< Thread local storage <<<<<<<<<<<<<<<<<<<<<<
extern TLS_KEY tlsKey;
extern TLS_KEY mutexPtrKey;

extern TLS_KEY vectorClockKey;
extern TLS_KEY tlsWriteSignatureKey;
extern TLS_KEY tlsReadSignatureKey;
// >>> Thread local storage >>>>>>>>>>>>>>>>>>>>>>

// <<< Global storage <<<<<<<<<<<<<<<<<<<<<<<<<<<<
extern UINT32 globalId;
extern PIN_LOCK lock;
// >>> Global storage >>>>>>>>>>>>>>>>>>>>>>>>>>>>

#endif
