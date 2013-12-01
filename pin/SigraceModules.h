#ifndef _SIGRACE_MODULES_H_
#define _SIGRACE_MODULES_H_

#include "pin.H"

#include <iostream>
#include <iterator>
#include <stdio.h>
#include <list>
#include <map>

#include "Bloom.h"
#include "VectorClock.h"

#define NO_ID ((UINT32) 0xFFFFFFFF)
#define BLOCK_HISTORY_QUEUE_SIZE 16

//typedef vector<UINT32> VectorClock;

/*
 * IMPLEMENTATION OF WAITING QUEUE
 */
typedef std::list<UINT32> WaitQueue;
typedef std::map<ADDRINT, WaitQueue*> WaitQueueMap;

typedef WaitQueueMap::iterator WaitQueueIterator;

typedef std::map<OS_THREAD_ID, THREADID> ThreadIdMap;
typedef ThreadIdMap::iterator ThreadIdMapItr;

class ThreadInfo
{
public:
	UINT32 tid;
	VectorClock vectorClock;

	ThreadInfo() :
			tid(NO_ID )
	{
	}

	ThreadInfo(UINT32 tid, const VectorClock& vc) :
			tid(tid), vectorClock(vc)
	{
	}

	ThreadInfo& operator=(const ThreadInfo& other)
	{
		ThreadInfo temp(other);
		tid = other.tid;
		vectorClock = other.vectorClock;

		return *this;
	}

	void update(UINT32 tid, const VectorClock& vc)
	{
		this->tid = tid;
		this->vectorClock = vc;
	}
};
typedef std::map<ADDRINT, ThreadInfo> UnlockThreadMap;
typedef UnlockThreadMap::iterator UnlockThreadIterator;

//typedef std::map< ADDRINT, list<ThreadInfo*>* > NotifyThreadMap;
typedef std::map<ADDRINT, ThreadInfo> NotifyThreadMap;
typedef NotifyThreadMap::iterator NotifyThreadIterator;

/*
 * IMPLEMENTATION OF THREAD SYNCHRONIZATION SIGNATURE
 */
class SigRaceData
{
public:
	SigRaceData(int tid, VectorClock& ts, Bloom& r, Bloom& w) :
			tid(tid), ts(ts), r(r), w(w)
	{
	}

	bool operator<(const SigRaceData& rhs)
	{
		return ts.happensBefore(rhs.ts);
	}

	bool isDirty()
	{
		return !r.isEmpty() || !w.isEmpty();
	}

	UINT32 tid;
	VectorClock ts;
	Bloom r;
	Bloom w;
};

/*
 * IMPLEMENTATION OF RACE DETECTION MODULE
 */
typedef std::list<SigRaceData*> BlockHistoryQueue;
class RaceDetectionModule
{
public:
	RaceDetectionModule() :
			threadCount(0)
	{
	}
	~RaceDetectionModule()
	{
		while (!blockHistoryQueues.empty())
		{
			BlockHistoryQueue* queue = blockHistoryQueues.back();
			blockHistoryQueues.pop_back();
			while (!queue->empty())
			{
				delete queue->front();
				queue->pop_front();
			}
			delete queue;
		}
	}

	void addSignature(SigRaceData* sigRaceData)
	{
		if (!sigRaceData->isDirty())
		{
			return;
		}

		// add it to the queue
		BlockHistoryQueue* queue = blockHistoryQueues[sigRaceData->tid];
		queue->push_front(sigRaceData);

		if (queue->size() > BLOCK_HISTORY_QUEUE_SIZE)
		{
			delete queue->back();
			queue->pop_back();
		}

		// check it with other threads' values
		for (UINT32 queueId = 0; queueId < blockHistoryQueues.size(); queueId++)
		{
			if (queueId == sigRaceData->tid)
			{
				continue;
			}
			queue = blockHistoryQueues[queueId];
			BlockHistoryQueue::iterator itr;
			SigRaceData* other;
			for (itr = queue->begin(); itr != queue->end(); itr++)
			{
				other = *itr;
				// rest is already HB this one
				if (*other < *sigRaceData)
				{
					break;
				}

				if (sigRaceData->r.hasInCommon(other->w))
				{
					fprintf(stderr,
							"THERE MAY BE A DATA RACE r-w BETWEEN THREAD-%d & THREAD-%d !!!\n",
							sigRaceData->tid, other->tid);
					fflush(stderr);
					goto OUTER_FOR;
				}
				else if (sigRaceData->w.hasInCommon(other->r))
				{
					fprintf(stderr,
							"THERE MAY BE A DATA RACE w-r BETWEEN THREAD-%d & THREAD-%d !!!\n",
							sigRaceData->tid, other->tid);
					fflush(stderr);
					goto OUTER_FOR;
				}
				else if (sigRaceData->w.hasInCommon(other->w))
				{
					fprintf(stderr,
							"THERE MAY BE A DATA RACE w-w BETWEEN THREAD-%d & THREAD-%d !!!\n",
							sigRaceData->tid, other->tid);
					fflush(stderr);
					goto OUTER_FOR;
				}
			}
		}
		OUTER_FOR: return;
	}

	void addProcessor()
	{
		threadCount++;
		blockHistoryQueues.push_back(new BlockHistoryQueue);
	}

private:
	std::vector<BlockHistoryQueue*> blockHistoryQueues;
	int threadCount;
};

#endif
