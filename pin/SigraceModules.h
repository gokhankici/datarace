#ifndef _SIGRACE_MODULES_H_
#define _SIGRACE_MODULES_H_

#include "pin.H"

#include <iostream>
#include <iterator>
#include <stdio.h>
#include <list>
#include <map>
#include <assert.h>

#include "Bloom.h"
#include "VectorClock.h"
#include "RecordNReplay.h"
#include "MyFlags.h"

#define NO_ID ((UINT32) 0xFFFFFFFF)
#define BLOCK_HISTORY_QUEUE_SIZE 16
#define MAX_THREAD_COUNT 32

extern PIN_LOCK fileLock;
extern PIN_LOCK rdmLock;

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
	{}

	ThreadInfo(UINT32 tid, const VectorClock& vc) :
			tid(tid), vectorClock(vc)
	{}

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

typedef std::map<ADDRINT, ThreadInfo> NotifyThreadMap;
typedef NotifyThreadMap::iterator NotifyThreadIterator;

class BarrierData
{
public:
	VectorClock vectorClock;
	VectorClock previousVectorClock;
	int barrierSize;
	int waiterCount;

	BarrierData(int barrierSize)
	{
		this->barrierSize = barrierSize;
		waiterCount = 0;
	}

	BarrierData()
	{
		barrierSize = 0;
		waiterCount = 0;
	}
};

typedef std::map<ADDRINT, BarrierData*> BarrierMap;
typedef BarrierMap::iterator BarrierMapItr;

/*
 * IMPLEMENTATION OF THREAD SYNCHRONIZATION SIGNATURE
 */
class SigRaceData
{
public:
	SigRaceData(int tid, VectorClock& ts, Bloom& r, Bloom& w) :
			tid(tid), ts(ts), r(r), w(w)
	{}

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
	{}
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

#ifdef PRINT_SIGNATURES
		cout << "---SIGNATURE" << endl << sigRaceData->ts <<
				"read: " << sigRaceData->r <<
				"write: " << sigRaceData->w << endl;
#endif

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
#ifdef PRINT_DETAILED_RACE_INFO

					fprintf(stderr, "Thread %d VC:\n", sigRaceData->tid);
					sigRaceData->ts.printVector(stderr);
					fprintf(stderr, "Thread %d VC:\n", other->tid);
					other->ts.printVector(stderr);
#endif

					fflush(stderr);

					goto OUTER_FOR;
				}
				else if (sigRaceData->w.hasInCommon(other->r))
				{
					fprintf(stderr,
					        "THERE MAY BE A DATA RACE w-r BETWEEN THREAD-%d & THREAD-%d !!!\n",
					        sigRaceData->tid, other->tid);
#ifdef PRINT_DETAILED_RACE_INFO

					fprintf(stderr, "Thread %d VC:\n", sigRaceData->tid);
					sigRaceData->ts.printVector(stderr);
					fprintf(stderr, "Thread %d VC:\n", other->tid);
					other->ts.printVector(stderr);
#endif

					fflush(stderr);
					goto OUTER_FOR;
				}
				else if (sigRaceData->w.hasInCommon(other->w))
				{
					fprintf(stderr,
					        "THERE MAY BE A DATA RACE w-w BETWEEN THREAD-%d & THREAD-%d !!!\n",
					        sigRaceData->tid, other->tid);
#ifdef PRINT_DETAILED_RACE_INFO

					fprintf(stderr, "Thread %d VC:\n", sigRaceData->tid);
					sigRaceData->ts.printVector(stderr);
					fprintf(stderr, "Thread %d VC:\n", other->tid);
					other->ts.printVector(stderr);
#endif

					fflush(stderr);
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
		assert(threadCount < MAX_THREAD_COUNT);
		blockHistoryQueues.push_back(new BlockHistoryQueue);
	}

private:
	void printRaceInfo(string type, int thread1, int thread2)
	{
		ADDRINT insPtr = 0; // get this while instrumenting
		int col = 0;
		int lineNumber = 0;
		std::string fileName;

		fprintf(stderr,
		        "-----------------------RACE INFO STARTS-----------------------------");

		GetLock(&fileLock, PIN_ThreadId() + 1);
		fprintf(stderr,
		        "There may be a data race (%s) between thread-%d & thread-%d !!!\n",
		        type.c_str(), thread1, thread2);
		PIN_GetSourceLocation(insPtr, &col, &lineNumber, &fileName);
		fprintf(stderr, "The Exact Place: %s @ %d\n", fileName.c_str(),
		        lineNumber);
		fprintf(stderr,
		        "-----------------------RACE INFO ENDS-------------------------------");
		fflush(stderr);
	}

	std::vector<BlockHistoryQueue*> blockHistoryQueues;
	int threadCount;
};

/*
 * MEMORY AREA USED FOR HANDLING MALLOC/FREE ISSUES
 */
class MemoryArea
{
public:
	THREADID tid;
	ADDRINT from; // inclusive
	ADDRINT to;   // exclusive

	MemoryArea() :
			tid(0), from(0), to(0)
	{}

	MemoryArea(THREADID tid, ADDRINT from, ADDRINT to) :
			tid(tid), from(from), to(to)
	{}

	MemoryArea(const MemoryArea& rhs)
	{
		tid = rhs.tid;
		from = rhs.from;
		to = rhs.to;
	}

	bool operator<(const MemoryArea& rhs) const
	{
		return to <= rhs.from;
	}

	bool operator==(const MemoryArea& rhs) const
	{
		// only from is required when looking for equality ?
		return from == rhs.from;
	}

	const MemoryArea& operator=(const MemoryArea& rhs)
	{
		tid = rhs.tid;
		from = rhs.from;
		to = rhs.to;
		return *this;
	}

	bool overlaps(const MemoryArea& mem)
	{
		// not ( this is after mem || this is before mem )
		return !(from >= mem.to || to <= mem.from);
	}

	ADDRINT size() const
	{
		return to - from;
	}

};
typedef std::set<MemoryArea> MemorySet;
typedef MemorySet::iterator MemorySetItr;

typedef std::map<ADDRINT, VectorClock> CollectiveBarrierVCMap;
typedef CollectiveBarrierVCMap::iterator CollectiveBarrierVCMapItr;

#endif
