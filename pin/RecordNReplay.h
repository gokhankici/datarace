/*
 * RecordNReplay.h
 *
 *  Created on: Dec 27, 2013
 *      Author: gokhan
 */

#ifndef RECORDNREPLAY_H_
#define RECORDNREPLAY_H_

#include "pin.H"

extern UINT32 globalEventId;
extern PIN_LOCK recordLock;
extern FILE* recordFile;
extern FILE* raceInfoFile;

typedef enum
{
	CREATE, LOCK, UNLOCK, COND_WAIT, COND_SIGNAL, COND_BROADCAST, BARRIER_WAIT
} OperationType;

class CreateInfo
{
public:
	THREADID tid;
	THREADID parent;

	CreateInfo(THREADID tid, THREADID parent) :
			tid(tid), parent(parent)
	{
	}

	bool operator <(const CreateInfo& rhs) const
	{
		return (tid < rhs.tid);
	}
};
typedef vector<CreateInfo> ThreadCreateOrder;
typedef vector<CreateInfo>::iterator ThreadCreateOrderItr;
extern ThreadCreateOrder threadCreateOrder;
extern FILE* createFile;

VOID PrintRecordInfo(THREADID tid, OperationType type);

#endif /* RECORDNREPLAY_H_ */
