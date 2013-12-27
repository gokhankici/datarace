/*
 * RecordNReplay.c
 *
 *  Created on: Dec 27, 2013
 *      Author: gokhan
 */

#include "pin.H"
#include "RecordNReplay.h"

#include <stdio.h>
#include <assert.h>

VOID PrintRecordInfo(THREADID tid, OperationType type)
{
	GetLock(&recordLock, tid + 1);

	UINT32 eventId = ++globalEventId;
	assert(recordFile);
	fprintf(recordFile, "%d %d %d\n", eventId, tid, type);

	ReleaseLock(&recordLock);
}
