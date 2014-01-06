#include "VectorClock.h"
#include "GlobalVariables.h"

#include <iomanip>
#include <stdio.h>
#include <string.h>
#include <assert.h>

int VectorClock::totalProcessCount = MAX_THREAD_COUNT;
int VectorClock::totalDeletedLockCount = 0;

// used for backwards compatibility purposes only
VectorClock::VectorClock()
{
	threadId = NON_THREAD_VECTOR_CLOCK;

	size_t byteCount = sizeof(int) * totalProcessCount;
	vc = (UINT32*) malloc(byteCount);
	memset(vc, 0, byteCount);
}

VectorClock::VectorClock(int processId)
{
	threadId = processId;
	assert(threadId < totalProcessCount);

	size_t byteCount = sizeof(int) * totalProcessCount;
	vc = (UINT32*) malloc(byteCount);
	memset(vc, 0, byteCount);

	if (threadId != NON_THREAD_VECTOR_CLOCK)
		vc[processId] = 1;
}

/*
 * Initialize vector clock from an existing clock
 */
VectorClock::VectorClock(VectorClock& vectorClock, int processId)
{
	*this = vectorClock;
	this->threadId = processId;

	// increment values to create a happens before relationship
	vc[threadId]++;
	vc[vectorClock.threadId]++;
}

VectorClock::VectorClock(const VectorClock& copyVC)
{
	threadId = copyVC.threadId;
	size_t byteCount = sizeof(int) * totalProcessCount;
	vc = (UINT32*) malloc(byteCount);
	memcpy(vc, copyVC.vc, byteCount);
}

VectorClock::VectorClock(istream& in, int processId)
{
	threadId = processId;
	assert(threadId < totalProcessCount);

	size_t byteCount = sizeof(int) * totalProcessCount;
	vc = (UINT32*) malloc(byteCount);
	memset(vc, 0, byteCount);

	int i = 0;
	string s;

	while (in)
	{
		if (!getline( in, s, ',' ))
		{
			break;
		}

		assert(i < totalProcessCount);
		vc[i++] = atoi(s.c_str());
	}

	assert(i == totalProcessCount);
}

const VectorClock& VectorClock::operator=(const VectorClock& vcRight)
{
	threadId = vcRight.threadId;
	size_t byteCount = sizeof(int) * totalProcessCount;
	vc = (UINT32*) malloc(byteCount);
	memcpy(vc, vcRight.vc, byteCount);

	return *this;
}

VectorClock::~VectorClock()
{
	totalDeletedLockCount++;
	free(vc);
}

void VectorClock::sendEvent()
{
	advance();
}

void VectorClock::set(int index, UINT32 value)
{
	assert(index < totalProcessCount);
	vc[index] = value;
}

UINT32 VectorClock::get()
{
	assert(threadId != NON_THREAD_VECTOR_CLOCK);
	return vc[threadId];
}

void VectorClock::clear()
{
	memset(vc, 0, totalProcessCount * sizeof(UINT32));
}

void VectorClock::advance()
{
	vc[threadId]++;
}

void VectorClock::receiveAction(VectorClock& vectorClockReceived)
{
	UINT32 *receivedClockValues = vectorClockReceived.vc;
	for (int i = 0; i < totalProcessCount; ++i)
		vc[i] = (vc[i] > receivedClockValues[i]) ?
		        vc[i] : receivedClockValues[i];

}

void VectorClock::receiveActionFromSpecialPoint(
    VectorClock& vectorClockReceived, UINT32 specialPoint)
{
	UINT32 *receivedClockValues = vectorClockReceived.vc;
	vc[specialPoint] = receivedClockValues[specialPoint];
}

bool VectorClock::happensBefore(const VectorClock& input)
{
	return (*this < input);
}

bool VectorClock::isUniqueValue(int processIdIn)
{
	bool isUnique = true;
	for (int i = 0; i < totalProcessCount; ++i)
	{
		if (vc[i] > 0 && i != processIdIn)
		{
			isUnique = false;
			break;
		}
	}

	return isUnique;
}

bool VectorClock::isEmpty()
{
	for (int i = 0; i < totalProcessCount; ++i)
	{
		if (vc[i] != 0)
			return false;
	}

	return true;
}

bool VectorClock::happensBeforeSpecial(const VectorClock* input,
                                       UINT32 processId)
{
	UINT32* vRightValues = input->vc;
	for (int i = 0; i < totalProcessCount; ++i)
	{
		if ((uint32_t) i == processId)
			continue;

		//at least ONE value is stricly smaller
		if (vc[i] > 0 && vc[i] >= vRightValues[i])
			return true;
	}

	//if there happened strictlySmaller, then smaller operation returns true;
	return false;
}

bool VectorClock::areConcurrent(VectorClock& input, ADDRINT processId)
{
	return !happensBefore(input) && !input.happensBefore(*this);
}

void VectorClock::toString()
{
	//cout << "segmentId:" << processId << "\t";
	//for (int i=0; i < totalProcessCount; ++i)
	//cout << v[i] << " " ;
	//cout << endl;
}

ostream& operator<<(ostream& os, const VectorClock& v)
{
	os << "Vector Clock Of " << v.threadId << ":" << endl;
	UINT32 *values = v.vc;
	for (int i = 0; i < v.totalProcessCount - 1; ++i)
	{
		os << values[i] << ",";
	}
	os << values[v.totalProcessCount - 1] << endl;

	return os;
}

VectorClock& VectorClock::operator++()
{
	vc[threadId]++;
	return *this;
}

VectorClock VectorClock::operator++(int)
{
	VectorClock tmp = *this;
	vc[threadId]++;
	return tmp;
}

bool VectorClock::operator<=(const VectorClock& vRight)
{
	return operator<(vRight) || operator==(vRight);
}

/*
 bool VectorClock::operator<(const VectorClock& vRight)
 {
 bool strictlySmaller = false;
 UINT32* vRightValues = vRight.getValues();
 for (int i = 0; i < totalProcessCount; ++i)
 {
 //at least ONE value is stricly smaller
 if (v[i] < vRightValues[i])
 strictlySmaller = true;
 //if any value of v[i] is greater, than no way v<vRight
 else if (v[i] > vRightValues[i])
 return false;
 }

 //if there happened strictlySmaller, then smaller operation returns true;
 return strictlySmaller;
 }
 */

bool VectorClock::operator<(const VectorClock& vRight)
{
	if (threadId == vRight.threadId)
	{
		return vc[threadId] < vRight.vc[vRight.threadId];
	}

	UINT32 l_l = vc[threadId];
	UINT32 l_r = vc[vRight.threadId];
	UINT32 r_r = vRight.vc[vRight.threadId];
	UINT32 r_l = vRight.vc[threadId];

	// both threads' values are lower in the lhs thread
	return l_l < r_l && l_r < r_r;
}

bool VectorClock::operator!=(const VectorClock& vRight)
{
	return !(operator==(vRight));
}

bool VectorClock::operator==(const VectorClock& vRight)
{
	UINT32 *vRightValues = vRight.vc;
	for (int i = 0; i < totalProcessCount; ++i)
		if (vc[i] != vRightValues[i])
			return false;

	return true;
}

void VectorClock::printVector(FILE* out)
{
	for (int i = 0; i < totalProcessCount - 1; ++i)
	{
		fprintf(out, "%d,", vc[i]);
	}
	fprintf(out, "%d\n", vc[totalProcessCount - 1]);
}
