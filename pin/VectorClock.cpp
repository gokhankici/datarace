#include "VectorClock.h"
#include "GlobalVariables.h"

#include <iomanip>
#include <stdio.h>
#include <string.h>

int VectorClock::totalProcessCount = MAX_VC_SIZE;
int VectorClock::totalDeletedLockCount = 0;

// used for backwards compatibility purposes only
VectorClock::VectorClock()
{
	threadId = NON_THREAD_VECTOR_CLOCK;

	size_t byteCount = sizeof(int) * totalProcessCount;
	v = (UINT32*) malloc(byteCount);
	memset(v, 0, byteCount);
}

VectorClock::VectorClock(int processIdIn)
{
	threadId = processIdIn;

	size_t byteCount = sizeof(int) * totalProcessCount;
	v = (UINT32*) malloc(byteCount);
	memset(v, 0, byteCount);

	if (threadId != NON_THREAD_VECTOR_CLOCK)
		v[processIdIn] = 1;
}

/*
 * Initialize vector clock from an existing clock
 */
VectorClock::VectorClock(VectorClock& vectorClock, int processId)
{
	*this = vectorClock;
	this->threadId = processId;

	// increment values to create a happens before relationship
	v[threadId]++;
	v[vectorClock.threadId]++;
}

VectorClock::VectorClock(const VectorClock& copyVC)
{
	threadId = copyVC.threadId;
	size_t byteCount = sizeof(int) * totalProcessCount;
	v = (UINT32*) malloc(byteCount);
	memcpy(v, copyVC.v, byteCount);
}

const VectorClock& VectorClock::operator=(const VectorClock& vcRight)
{
	threadId = vcRight.threadId;
	size_t byteCount = sizeof(int) * totalProcessCount;
	v = (UINT32*) malloc(byteCount);
	memcpy(v, vcRight.v, byteCount);

	return *this;
}

VectorClock::~VectorClock()
{
	totalDeletedLockCount++;
	free(v);
}

void VectorClock::sendEvent()
{
	advance();
}

void VectorClock::advance()
{
	v[threadId]++;
}

UINT32* VectorClock::getValues() const
{
	return v;
}

void VectorClock::receiveAction(VectorClock& vectorClockReceived)
{
	UINT32 *receivedClockValues = vectorClockReceived.getValues();
	for (int i = 0; i < totalProcessCount; ++i)
		v[i] = (v[i] > receivedClockValues[i]) ? v[i] : receivedClockValues[i];

}

void VectorClock::receiveActionFromSpecialPoint(
		VectorClock& vectorClockReceived, UINT32 specialPoint)
{
	UINT32 *receivedClockValues = vectorClockReceived.getValues();
	v[specialPoint] = receivedClockValues[specialPoint];
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
		if (v[i] > 0 && i != processIdIn)
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
		if (v[i] != 0)
			return false;
	}

	return true;
}

bool VectorClock::happensBeforeSpecial(const VectorClock* input,
		UINT32 processId)
{
	UINT32* vRightValues = input->getValues();
	for (int i = 0; i < totalProcessCount; ++i)
	{
		if ((uint32_t) i == processId)
			continue;

		//at least ONE value is stricly smaller
		if (v[i] > 0 && v[i] >= vRightValues[i])
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
	UINT32 *values = v.getValues();
//	for (int i = 0; i < v.totalProcessCount; ++i)
	for (int i = 0; i < 3; ++i)
	{
		os << values[i] << " - ";
	}
	os << endl;

	return os;
}

VectorClock& VectorClock::operator++()
{
	v[threadId]++;
	return *this;
}

VectorClock VectorClock::operator++(int)
{
	VectorClock tmp = *this;
	v[threadId]++;
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
		return v[threadId] < vRight.v[vRight.threadId];
	}

	UINT32 l_l = v[threadId];
	UINT32 l_r = v[vRight.threadId];
	UINT32 r_r = vRight.v[vRight.threadId];
	UINT32 r_l = vRight.v[threadId];

	// both threads' values are lower in the lhs thread
	return l_l < r_l && l_r < r_r;
}

bool VectorClock::operator!=(const VectorClock& vRight)
{
	return !(operator==(vRight));
}

bool VectorClock::operator==(const VectorClock& vRight)
{
	UINT32 *vRightValues = vRight.getValues();
	for (int i = 0; i < totalProcessCount; ++i)
		if (v[i] != vRightValues[i])
			return false;

	return true;
}

void VectorClock::printVector(FILE* out)
{
	fprintf(out, "Vector Clock Of %d:\n", threadId);
	for (int i = 0; i < totalProcessCount; ++i)
	{
		fprintf(out, "%d - ", v[i]);
	}
	fprintf(out, "\n");
}
