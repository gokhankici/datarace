#ifndef _SIGRACE_MODULES_H_
#define _SIGRACE_MODULES_H_

#include <iostream>
#include <iterator>
#include <stdio.h>
#include <list>
#include <map>

#include "Bloom.h"

#define NO_ID ((UINT32) 0xFFFFFFFF)
#define BLOCK_HISTORY_QUEUE_SIZE 10

typedef vector<UINT32> VectorClock;

/*
 * IMPLEMENTATION OF WAITING QUEUE
 */
typedef std::list<UINT32> WaitQueue;
typedef std::map< long, WaitQueue* > WaitQueueMap;
typedef WaitQueueMap::iterator WaitQueueIterator;

class SignalThreadInfo 
{
	public:
		UINT32 threadId;
		VectorClock vectorClock;

		SignalThreadInfo() : threadId(NO_ID) {}

		SignalThreadInfo (UINT32 threadId, const VectorClock& vc) :
			threadId(threadId), vectorClock(vc) {}

		SignalThreadInfo& operator= (const SignalThreadInfo& other)
		{
			SignalThreadInfo temp (other);
			threadId = other.threadId;
			vectorClock = other.vectorClock;

			return *this;
		}

		void update(UINT32 tid, const VectorClock& vc)
		{
			threadId    = tid;
			vectorClock = vc;
		}
};
typedef std::map< long, SignalThreadInfo > SignalThreadMap;
typedef SignalThreadMap::iterator SignalThreadIterator;

/*
 * IMPLEMENTATION OF THREAD SYNCHRONIZATION SIGNATURE
 */
class SigRaceData
{
	public:
		SigRaceData(int threadId, VectorClock& ts, Bloom& r, Bloom& w)
			: threadId(threadId), ts(ts), r(r), w(w) {}

		bool operator<(const SigRaceData& rhs )
		{
			if (threadId == rhs.threadId) 
			{
				return ts[threadId] < rhs.ts[rhs.threadId];
			}

			UINT32 l_l = ts[threadId];
			UINT32 l_r = ts[rhs.threadId];
			UINT32 r_r = rhs.ts[rhs.threadId];
			UINT32 r_l = rhs.ts[threadId];

			// both threads' values are lower in the lhs thread
			return l_l < r_l && l_r < r_r;
		}

		UINT32 threadId;
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
		RaceDetectionModule() : threadCount(0) { }
		~RaceDetectionModule()
		{
			while(!blockHistoryQueues.empty())
			{
				BlockHistoryQueue* queue = blockHistoryQueues.back();
				blockHistoryQueues.pop_back();
				while(!queue->empty()) 
				{
					delete queue->front();
					queue->pop_front();
				}
				delete queue;
			}
		}

		void addSignature(SigRaceData* sigRaceData)
		{
			// add it to the queue
			BlockHistoryQueue* queue = blockHistoryQueues[sigRaceData->threadId];
			queue->push_front(sigRaceData);

			if (queue->size() > BLOCK_HISTORY_QUEUE_SIZE) 
			{
				delete queue->back();
				queue->pop_back();
			}

			// check it with other threads' values
			for (UINT32 queueId = 0; queueId < blockHistoryQueues.size(); queueId++) 
			{
				if(queueId == sigRaceData->threadId)
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
						printf("THERE MAY BE A DATA RACE r-w BETWEEN THREAD-%d & THREAD-%d !!!\n", sigRaceData->threadId, other->threadId);
						goto OUTER_FOR;
					}
					else if (sigRaceData->w.hasInCommon(other->r)) 
					{
						printf("THERE MAY BE A DATA RACE w-r BETWEEN THREAD-%d & THREAD-%d !!!\n", sigRaceData->threadId, other->threadId);
						goto OUTER_FOR;
					}
					else if (sigRaceData->w.hasInCommon(other->w)) 
					{
						printf("THERE MAY BE A DATA RACE w-w BETWEEN THREAD-%d & THREAD-%d !!!\n", sigRaceData->threadId, other->threadId);
						goto OUTER_FOR;
					}
				}
			}
			OUTER_FOR:
			return;
		}

		void addProcessor()
		{
			threadCount++;
			blockHistoryQueues.push_back(new BlockHistoryQueue);
		}

	private:
		std::vector< BlockHistoryQueue* > blockHistoryQueues;
		int threadCount;
};

#endif
