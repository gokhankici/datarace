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

#ifdef IN_REPLAY_TOOL

typedef std::map<OS_THREAD_ID, THREADID> ThreadIdMap;
typedef ThreadIdMap::iterator ThreadIdMapItr;

#endif

VOID PrintRecordInfo(THREADID tid, OperationType type);

#endif /* RECORDNREPLAY_H_ */
