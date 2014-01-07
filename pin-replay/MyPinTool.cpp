#include <stdio.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>
#include "pin.H"
#include <semaphore.h>

/* ### MY ADDITIONS ################################################ */
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "../pin/RecordNReplay.h"
#include "../pin/VectorClock.h"
#include "../pin/SigraceModules.h"
/* ### MY ADDITIONS ################################################ */

/* KNOB parameters */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "output",
                            "specify output file name");

KNOB<string> KnobMode(KNOB_MODE_WRITEONCE, "pintool", "mode", "replay",
                      "specify mode");

KNOB<string> KnobCreateFile(KNOB_MODE_WRITEONCE, "pintool", "createFile",
                            "../pin/create.txt",
                            "specify create file to order the thread creations");

KNOB<string> KnobEpochFile(KNOB_MODE_WRITEONCE, "pintool", "epochFile",
                           "../pin/thread_epochs.",
                           "specify create file to order the thread creations");

INT32 Usage()
{
	cerr << "This tool replays a multithread program." << endl;
	cerr << KNOB_BASE::StringKnobSummary();
	cerr << endl;
	return 1;
}


/* ### MY DEFINITIONS ################################################ */
// enable debug messages
// TODO: remove this later
//#define ENABLE_REEXECUTE_DEBUG

// extra verification by calculating the vector clock explicitly
// and comparing it with TRT
#define CALCULATE_VECTOR_CLOCK

// do the GRT >= TRT check
// TODO: remove this later
#define ENABLE_GRT_CHECK
/* ### MY DEFINITIONS ################################################ */


/* ### MY ADDITIONS ################################################ */
typedef deque<VectorClock> EpochList;
typedef EpochList::iterator EpochListIterator;

class ThreadLocalStorage
{
public:
	FILE* out;                        // ?????
	EpochList epochList;              // Epoch history coming from normal execution
	VectorClock currentVC;            // Current vector clock of the thread
	VectorClock TRT;                  // Next vector clock of the thread
	deque<VectorClock> createVCList;  // List of vc taken at thread creation times

	ThreadLocalStorage()
	{
		out = NULL;
	}

	~ThreadLocalStorage()
	{
		// make sure the file is closed
		if(out)
		{
			fclose(out);
		}
	}
};

TLS_KEY tlsKey;

inline static
ThreadLocalStorage* getTLS(THREADID tid)
{
	return static_cast<ThreadLocalStorage*> (PIN_GetThreadData(tlsKey, tid));
}

/*
 * Advances the TRT and returns true if TRT is successfully advanced
 */
inline static
bool advanceTRT(ThreadLocalStorage* tls)
{
	assert(tls);
	if(! tls->epochList.empty())
	{
		tls->currentVC = tls->TRT;

		EpochListIterator itr = tls->epochList.begin();
		tls->TRT = *itr;

		tls->epochList.pop_front();
		return true;
	}
	return false;
}

ThreadCreateOrder threadCreateOrder;
ThreadCreateOrderItr currentCreateOrder;

ThreadIdMap threadIdMap;

// Global Re-execution Timestamp
VectorClock GRT;
PIN_LOCK GRTLock;

/* ### MY ADDITIONS ################################################ */

typedef pair<UINT32, pair<UINT32, SIZE> > RECORD_PAIR;

const UINT32 SLEEP_TIME = 10;

BOOL enable_tool = false;

PIN_LOCK vector_lock;
PIN_LOCK mutex_map_lock;
PIN_LOCK cond_map_lock;
PIN_LOCK barrier_map_lock;
PIN_LOCK id_lock;
PIN_LOCK out_lock;

PIN_LOCK threadStartLock;
PIN_LOCK index_lock;
PIN_LOCK atomic_create;

VectorClock current_vc[MAX_THREAD_COUNT];
UINT32 global_id = 0;
UINT32 global_index = 0;

map<pthread_mutex_t*, VectorClock> mutex_map;
map<pthread_cond_t*, VectorClock> cond_map;
map<pthread_barrier_t*, deque<VectorClock>* > barrier_map;
vector<RECORD_PAIR> index_queue;
vector<RECORD_PAIR> record_vector;

/*
inline void getmyid(UINT32*newid)
{
	const pthread_t myid = mypthread_self();

	FPTHREAD_MUTEX_LOCK(&id_lock);
	UINT32 mytid = 0;
	map<pthread_t, UINT32>::iterator itr;
	if (global_id == 0)
	{
		if ((itr = id_map.find(myid)) == id_map.end())
		{
			id_map[myid] = 0;
			mytid = 0;
		}
		else
		{
			mytid = itr->second;
		}
		(*newid) = mytid;
	}
	else
	{
		while ((itr = id_map.find(myid)) == id_map.end())
		{
			FPTHREAD_MUTEX_UNLOCK(&id_lock);
			PIN_Sleep(SLEEP_TIME);
			FPTHREAD_MUTEX_LOCK(&id_lock);
		}

		mytid = itr->second;
		(*newid) = mytid;
	}

	FPTHREAD_MUTEX_UNLOCK(&id_lock);
}

//will have index_lock when returned
inline void waitTurn(const UINT32 mytid)
{
	pair<UINT32, SIZE> temp_pair;
	while (true)
	{
		FPTHREAD_MUTEX_LOCK(&index_lock);
		temp_pair = (index_queue.at(global_index)).second;
		if (temp_pair.first == mytid)
			break;
		else
		{
			FPTHREAD_MUTEX_UNLOCK(&index_lock);
			PIN_Sleep(SLEEP_TIME);
		}
	}
}

inline UINT32 getCurrentEventAndAdvance()
{
	FPTHREAD_MUTEX_LOCK(&index_lock);
	UINT32 ts = global_index;
	++global_index;
	FPTHREAD_MUTEX_UNLOCK(&index_lock);
	return ts;
}

inline void addToVector(const UINT32 global_index, const UINT32 mytid,
                        const SIZE op)
{
	RECORD_PAIR r;
	r.first = global_index;
	r.second.first = mytid;
	r.second.second = op;
	FPTHREAD_MUTEX_LOCK(&out_lock);
	record_vector.push_back(r);
	FPTHREAD_MUTEX_UNLOCK(&out_lock);
}

void logToFile(const char * name, const vector<RECORD_PAIR> & v, THREADID tid, PIN_LOCK* lock)
{
	RECORD_PAIR r;
	ofstream OutFile(name, ofstream::out | ofstream::trunc);
	GetLock(lock, tid + 1);
	int len = v.size();
	OutFile << len << '\n';
	for (int i = 0; i < len; i++)
	{
		r = v.at(i);
		OutFile << r.first << ' ' << r.second.first << ' ' << r.second.second
		<< '\n';
	}
	ReleaseLock(lock);
	OutFile.close();
}

inline void record(const UINT32 mytid, const SIZE op)
{
	UINT32 ts = getCurrentEventAndAdvance();
	addToVector(ts, mytid, op);
}

//will have index_lock when returned
inline void replay(const UINT32 mytid, const SIZE op)
{
	waitTurn(mytid);
	addToVector(global_index, mytid, op);
	++global_index;
}


void* thread_function(void* arg)
{
    puts("IN threaed_function YE-HOOO!\n");

    UINT32 mytid;
    getmyid(&mytid);

    create_argument * carg = (create_argument*) arg;
    UINT32 parentid = carg->parentid;

    FPTHREAD_MUTEX_LOCK(&vector_lock);
    current_vc[parentid].tick();
    FPTHREAD_MUTEX_UNLOCK(&vector_lock);

    //assign new vector clock
    current_vc[mytid].threadId = mytid;
    current_vc[mytid].receiveAction(&current_vc[parentid]);
    current_vc[mytid].tick();

    void* res = carg->fnc(carg->arg); //actual call to function

    current_vc[mytid].tick();

    FPTHREAD_MUTEX_LOCK(&vector_lock);
    current_vc[parentid].receiveAction(&current_vc[mytid]);
    current_vc[parentid].tick();
    FPTHREAD_MUTEX_UNLOCK(&vector_lock);

    return res;
}
*/



VOID BeforeCreate(THREADID tid, pthread_t *__restrict __newthread,
                  __const pthread_attr_t *__restrict __attr,
                  void *(*__start_routine)(void *), void *__restrict __arg)
{
	if (!enable_tool)
		return;

	//this is slighly different than others
	pair<UINT32, SIZE> temp_pair;

#ifdef ENABLE_GRT_CHECK

	ThreadLocalStorage* tls = getTLS(tid);
	assert(tls);
#endif

	while (true)
	{
		GetLock(&atomic_create, tid + 1);
		GetLock(&GRTLock, tid + 1);

		assert(currentCreateOrder != threadCreateOrder.end());
		CreateInfo ci = *currentCreateOrder;

#ifdef ENABLE_GRT_CHECK

#ifdef ENABLE_REEXECUTE_DEBUG

		cout << "Create\nGRT: " << GRT << "TRT: " << tls->TRT << endl;
#endif

		if (tid == ci.parent && tls->TRT.lessThanGRT(GRT))
		{
			break;
		}
#else

		if (tid == ci.parent)
		{
			break;
		}
#endif
		else
		{
			ReleaseLock(&GRTLock);
			ReleaseLock(&atomic_create);
			PIN_Sleep(SLEEP_TIME);
		}
	}

	tls->createVCList.push_back(tls->currentVC);

#ifdef ENABLE_GRT_CHECK

	GRT.updateGRT(tls->TRT);
#ifdef ENABLE_REEXECUTE_DEBUG

	cout << "GRT after create: \n" << GRT << endl;
#endif
#endif

	advanceTRT(tls);

	ReleaseLock(&GRTLock);
}

VOID AfterCreate(THREADID tid, int rc)
{
	currentCreateOrder++;
	ReleaseLock(&atomic_create);
}

VOID BeforeJoin(THREADID tid, pthread_t thread, void **retval)
{
	ThreadLocalStorage* tls = getTLS(tid);
	assert(tls);

#ifdef ENABLE_GRT_CHECK
	GetLock(&GRTLock, tid + 1);
	assert(tls->TRT.lessThanGRT(GRT));
	GRT.updateGRT(tls->TRT);
	ReleaseLock(&GRTLock);
#endif

	advanceTRT(tls);
}

/*
int mypthread_mutex_lock(pthread_mutex_t * __mutex)
{
    if (!enable_tool)
        return FPTHREAD_MUTEX_LOCK(__mutex);

    UINT32 mytid;
    getmyid(&mytid);
    int res = 0;
    if (KnobMode.Value() == "replay")
    {
        replay(mytid, 1);
        res = FPTHREAD_MUTEX_LOCK(__mutex);
        FPTHREAD_MUTEX_UNLOCK(&index_lock);
    }
    else if (KnobMode.Value() == "record")
    {
        res = FPTHREAD_MUTEX_LOCK(__mutex);
        record(mytid, 1);
    }

    current_vc[mytid].tick();

    FPTHREAD_MUTEX_LOCK(&mutex_map_lock);
    map<pthread_mutex_t*, VectorClock>::iterator itr = mutex_map.find(__mutex);
    if (itr != mutex_map.end())
    {
        if (itr->second.threadId != NO_ID)
        {
            current_vc[mytid].receiveAction(&(itr->second));
            itr->second.threadId = NO_ID;
        }
    }
    FPTHREAD_MUTEX_UNLOCK(&mutex_map_lock);

    return res;
}

int mypthread_mutex_unlock(pthread_mutex_t * __mutex)
{
    if (!enable_tool)
        return FPTHREAD_MUTEX_UNLOCK(__mutex);

    UINT32 mytid;
    getmyid(&mytid);
    int res = 0;

    current_vc[mytid].tick();

    FPTHREAD_MUTEX_LOCK(&mutex_map_lock);
    mutex_map[__mutex] = current_vc[mytid];
    FPTHREAD_MUTEX_UNLOCK(&mutex_map_lock);

    if (KnobMode.Value() == "replay")
    {
        replay(mytid, 2);
        res = FPTHREAD_MUTEX_UNLOCK(__mutex);
        FPTHREAD_MUTEX_UNLOCK(&index_lock);
    }
    else if (KnobMode.Value() == "record")
    {
        record(mytid, 2);
        res = FPTHREAD_MUTEX_UNLOCK(__mutex);
    }
    return res;
}

int mypthread_cond_wait(pthread_cond_t *__restrict __cond,
                        pthread_mutex_t *__restrict __mutex)
{
    if (!enable_tool)
        return FPTHREAD_COND_WAIT(__cond, __mutex);

    UINT32 mytid;
    getmyid(&mytid);

    current_vc[mytid].tick();

    FPTHREAD_MUTEX_LOCK(&mutex_map_lock);
    mutex_map[(pthread_mutex_t *) __mutex] = current_vc[mytid];
    FPTHREAD_MUTEX_UNLOCK(&mutex_map_lock);

    if (KnobMode.Value() == "record")
    {
        record(mytid, 3);
    }
    else if (KnobMode.Value() == "replay")
    {
        replay(mytid, 3);
        FPTHREAD_MUTEX_UNLOCK(&index_lock);
    }

    int res = FPTHREAD_COND_WAIT(__cond, __mutex);

    FPTHREAD_MUTEX_LOCK(&cond_map_lock);
    map<pthread_cond_t*, VectorClock>::iterator itr = cond_map.find(
                (pthread_cond_t *) __cond);
    if (itr != cond_map.end())
    {
        if (itr->second.threadId != NO_ID)
        {
            current_vc[mytid].receiveAction(&(itr->second));
            itr->second.threadId = NO_ID;
        }
    }
    FPTHREAD_MUTEX_UNLOCK(&cond_map_lock);

    if (KnobMode.Value() == "record")
    {
        record(mytid, 3);
    }
    else if (KnobMode.Value() == "replay")
    {
        //this is slightly different than others
        pair<UINT32, SIZE> temp_pair;
        while (true)
        {
            FPTHREAD_MUTEX_LOCK(&index_lock);
            temp_pair = (index_queue.at(global_index)).second;
            if (temp_pair.first == mytid)
                break;
            else
            {
                FPTHREAD_MUTEX_UNLOCK(&index_lock);
                //force to release mutex
                FPTHREAD_MUTEX_UNLOCK(__mutex);
                PIN_Sleep(SLEEP_TIME);
                //acquire lock try again
                FPTHREAD_MUTEX_LOCK(__mutex);
            }
        }
        addToVector(global_index, mytid, 3);
        ++global_index;
        FPTHREAD_MUTEX_UNLOCK(&index_lock);
    }

    return res;
}

int mypthread_cond_signal(pthread_cond_t * __cond)
{
    if (!enable_tool)
        return FPTHREAD_COND_SIGNAL(__cond);

    UINT32 mytid;
    getmyid(&mytid);
    int res = 0;

    current_vc[mytid].tick();

    FPTHREAD_MUTEX_LOCK(&cond_map_lock);
    cond_map[__cond] = current_vc[mytid];
    FPTHREAD_MUTEX_UNLOCK(&cond_map_lock);

    if (KnobMode.Value() == "replay")
    {
        replay(mytid, 4);
        res = FPTHREAD_COND_SIGNAL(__cond);
        FPTHREAD_MUTEX_UNLOCK(&index_lock);
    }
    else if (KnobMode.Value() == "record")
    {
        record(mytid, 4);
        res = FPTHREAD_COND_SIGNAL(__cond);
    }

    return res;
}

int mypthread_cond_broadcast(pthread_cond_t * __cond)
{
    if (!enable_tool)
        return FPTHREAD_COND_BROADCAST(__cond);

    UINT32 mytid;
    getmyid(&mytid);
    int res = 0;

    current_vc[mytid].tick();

    FPTHREAD_MUTEX_LOCK(&cond_map_lock);
    cond_map[__cond] = current_vc[mytid];
    FPTHREAD_MUTEX_UNLOCK(&cond_map_lock);

    if (KnobMode.Value() == "replay")
    {
        replay(mytid, 5);
        res = FPTHREAD_COND_BROADCAST(__cond);
        FPTHREAD_MUTEX_UNLOCK(&index_lock);
    }
    else if (KnobMode.Value() == "record")
    {
        record(mytid, 5);
        res = FPTHREAD_COND_BROADCAST(__cond);
    }

    return res;
}

int mypthread_barrier_wait(pthread_barrier_t * __barrier)
{
    if (!enable_tool)
        return FPTHREAD_BARRIER_WAIT(__barrier);

    UINT32 mytid;
    getmyid(&mytid);

    FPTHREAD_MUTEX_LOCK(&barrier_map_lock);
    map<pthread_barrier_t*, deque<VectorClock>*>::iterator itr =
        barrier_map.find(__barrier);
    deque<VectorClock>* barrier_list = NULL;
    if (itr != barrier_map.end())
    {
        barrier_list = itr->second;
    }

    current_vc[mytid].tick();
    if (barrier_list != NULL)
    {
        barrier_list->push_back(current_vc[mytid]);
    }
    else
    {
        barrier_list = new deque<VectorClock>;
        barrier_list->push_back(current_vc[mytid]);
        barrier_map[__barrier] = barrier_list;
    }
    FPTHREAD_MUTEX_UNLOCK(&barrier_map_lock);

    if (KnobMode.Value() == "replay")
    {
        replay(mytid, 6);
        FPTHREAD_MUTEX_UNLOCK(&index_lock);
    }
    else if (KnobMode.Value() == "record")
    {
        record(mytid, 6);
    }

    int res = FPTHREAD_BARRIER_WAIT(__barrier);

    FPTHREAD_MUTEX_LOCK(&barrier_map_lock);
    itr = barrier_map.find(__barrier);
    barrier_list = NULL;
    if (itr != barrier_map.end())
    {
        barrier_list = itr->second;
    }
    //bool found = false;
    VectorClock myClock;
    deque<VectorClock>::iterator qitr = barrier_list->begin();
    for (; qitr != barrier_list->end(); ++qitr)
    {
        if ((*qitr).threadId == mytid)
        {
            myClock = (*qitr);
            barrier_list->erase(qitr);
            //found =true;
            break;
        }
    }

    qitr = barrier_list->begin();
    for (; qitr != barrier_list->end(); ++qitr)
    {
        myClock.receiveAction(&(*qitr));
        qitr->receiveAction(&myClock);
    }
    current_vc[mytid] = myClock;
    FPTHREAD_MUTEX_UNLOCK(&barrier_map_lock);

    if (KnobMode.Value() == "replay")
    {
        replay(mytid, 6);
        FPTHREAD_MUTEX_UNLOCK(&index_lock);
    }
    else if (KnobMode.Value() == "record")
    {
        record(mytid, 6);
    }

    return res;
}
*/

VOID BeforeMain(THREADID tid, int argc, char ** argv)
{
	//assign main thread vector clock
	current_vc[0].threadId = 0;
	current_vc[0].advance();

	/* MY ADDITIONS */
	FILE* createFile = fopen(KnobCreateFile.Value().c_str(), "r");

	THREADID created, parent;
	int readCount;

	while (!feof(createFile))
	{
		readCount = fscanf(createFile, "%d %d\n", &created, &parent);
		if (readCount != 2)
		{
			fprintf(stderr, "error occured while reading from create file %s\n",
			        KnobCreateFile.Value().c_str());
			exit(1);
		}
		threadCreateOrder.push_back(CreateInfo(created, parent));
	}
	currentCreateOrder  = threadCreateOrder.begin();
	/* MY ADDITIONS */

	enable_tool = true;
}

// used to define the point of instrumentation
#define INSTRUMENT_BEFORE 1
#define INSTRUMENT_AFTER  2
#define INSTRUMENT_BOTH   (INSTRUMENT_BEFORE | INSTRUMENT_AFTER)

VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
	ThreadLocalStorage* tls = new ThreadLocalStorage();

	// create the stream to read the epochs
	char epochFileName[30];
	sprintf(epochFileName, "%s%d", KnobEpochFile.Value().c_str(), tid);

	ifstream infile(epochFileName);

	// read epochs into memory
	while (infile)
	{
		string s;
		if (!getline( infile, s ))
		{
			break;
		}

		istringstream ss( s );
		vector <int> record;

		tls->epochList.push_back(VectorClock(ss, tid));
	}

	// load the first TRT
	bool emptyEpochListCheck = advanceTRT(tls);
	assert(emptyEpochListCheck);

	// Thread should be able to start
	GetLock(&GRTLock, tid + 1);

#ifdef ENABLE_REEXECUTE_DEBUG

	cout << "ThreadStart\nGRT: " << GRT << "TRT: " << tls->TRT;
#endif

	assert(tls->TRT.lessThanGRT(GRT));
	ReleaseLock(&GRTLock);

	GRT.updateGRT(tls->TRT);

	// update the TLS for upcoming sync operations
	tls->currentVC = tls->TRT;
	advanceTRT(tls);

#ifdef ENABLE_REEXECUTE_DEBUG

	cout << "next TLS after start: " << tls->TRT << endl;
#endif

#ifdef CALCULATE_VECTOR_CLOCK
	// check for correct thread id at creation
	threadIdMap[PIN_GetTid()] = tid;

	INT32 parentOS_TID = PIN_GetParentTid();
	if (parentOS_TID)
	{
		GetLock(&threadStartLock, tid + 1);

		// get parent tid from map
		ThreadIdMapItr parentTidItr = threadIdMap.find(parentOS_TID);
		assert(parentTidItr != threadIdMap.end());
		THREADID parentTID = parentTidItr->second;

		// calculate vector clock
		ThreadLocalStorage* parentTLS = getTLS(parentTID);
		assert(parentTLS);
		VectorClock calculatedVC(parentTLS->createVCList.front(), tid);
		parentTLS->createVCList.pop_front();

		// compare it with TRT
		assert(tls->currentVC == calculatedVC);

		ReleaseLock(&threadStartLock);
	}
#endif

	PIN_SetThreadData(tlsKey, tls, tid);
}

/* ===================================================================== */
/* Instrumnetation functions                                             */
/* ===================================================================== */

/*
 * Used to add instrumentation routines before and/or after a function.
 *
 * THREADID & all parameters are passed to the before routines.
 * THREADID & exit value are passed to the after routines.
 *
 * funptr:         function to add instrumentation
 * position:       before, after or both
 * parameterCount: # parameters to pass to the before instrumentation function
 *
 */
VOID
addInstrumentation(IMG img, const char * name, char position, int parameterCount, AFUNPTR beforeFUNPTR, AFUNPTR afterFUNPTR)
{
	RTN rtn = RTN_FindByName(img, name);
	if (RTN_Valid(rtn))
	{
		RTN_Open(rtn);

		if(position & INSTRUMENT_BEFORE)
		{
			assert(beforeFUNPTR);

			switch(parameterCount)
			{
			case 1:
				RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(beforeFUNPTR),
				               IARG_THREAD_ID,
				               IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
				               IARG_END);
				break;
			case 2:
				RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(beforeFUNPTR),
				               IARG_THREAD_ID,
				               IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
				               IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
				               IARG_END);
				break;
			case 3:
				RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(beforeFUNPTR),
				               IARG_THREAD_ID,
				               IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
				               IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
				               IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
				               IARG_END);
				break;
			case 4:
				RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(beforeFUNPTR),
				               IARG_THREAD_ID,
				               IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
				               IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
				               IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
				               IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
				               IARG_END);
				break;
			default:
				fprintf(stderr, "Instrumentation for functions with %d arguments are not implemented yet\n", parameterCount);
				exit(1);
			}
		}

		if(position & INSTRUMENT_AFTER)
		{
			assert(afterFUNPTR);

			RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(afterFUNPTR),
			               IARG_THREAD_ID,
			               IARG_FUNCRET_EXITPOINT_VALUE,
			               IARG_END);
		}

		RTN_Close(rtn);
	}
}

// Image load callback - inserts the probes.
void ImgLoad(IMG img, void *v)
{
	if ((IMG_Name(img).find("libpthread.so") != string::npos)
	        || (IMG_Name(img).find("LIBPTHREAD.SO") != string::npos)
	        || (IMG_Name(img).find("LIBPTHREAD.so") != string::npos))
	{
		addInstrumentation(img, "pthread_create", INSTRUMENT_BOTH, 4, AFUNPTR(BeforeCreate), AFUNPTR(AfterCreate));
	}

	if(IMG_IsMainExecutable(img))
	{
		addInstrumentation(img, "main", INSTRUMENT_BEFORE, 2, AFUNPTR(BeforeMain), NULL);
	}
}

/* ===================================================================== */
/* Main function                                                         */
/* ===================================================================== */

int main(int argc, char *argv[])
{
	// Initialize Pin
	PIN_InitSymbols();
	if (PIN_Init(argc, argv))
	{
		return Usage();
	}

	InitLock(&atomic_create);
	InitLock(&index_lock);
	InitLock(&threadStartLock);
	InitLock(&GRTLock);

	InitLock(&vector_lock);
	InitLock(&mutex_map_lock);
	InitLock(&cond_map_lock);
	InitLock(&barrier_map_lock);
	InitLock(&id_lock);
	InitLock(&out_lock);

	// Register the instrumentation callback
	IMG_AddInstrumentFunction(ImgLoad, 0);
	PIN_AddThreadStartFunction(ThreadStart, 0);

	// Start the application
	PIN_StartProgram();
	return 0;
}
