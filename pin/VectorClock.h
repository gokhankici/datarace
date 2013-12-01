/*
 * VectorClock.h
 *
 *  Created on: Apr 19, 2013
 *      Author: onder
 */

#ifndef VECTORCLOCK_H_
#define VECTORCLOCK_H_

#include "pin.H"

#define NON_THREAD_VECTOR_CLOCK -1

class VectorClock
{
private:
	UINT32* v;
public:
	int threadId;
	void advance();
	VectorClock();
	VectorClock(const VectorClock& copyVC);
	VectorClock(int processId);
	VectorClock(VectorClock& inClockPtr, int processId);
	~VectorClock();
	void toString();
	void receiveAction(VectorClock& vectorClockReceived);
	void receiveActionFromSpecialPoint(VectorClock& vectorClockReceived,
			UINT32 specialPoint);
	UINT32* getValues() const;
	void sendEvent();
	bool happensBefore(const VectorClock& input); //OK
	bool happensBeforeSpecial(const VectorClock* input, UINT32 processId);
	bool isUniqueValue(int processIdIn);
	bool isEmpty();

public:
	static int totalProcessCount;
	static int totalDeletedLockCount;
	VectorClock& operator++(); //prefix increment ++vclock
	VectorClock operator++(int x); //postfix increment
	const VectorClock& operator=(const VectorClock& vcRight);
	bool operator==(const VectorClock &vRight);
	bool operator!=(const VectorClock &vRight);
	bool operator<(const VectorClock& vRight);
	bool operator<=(const VectorClock& vRight);
	bool areConcurrent(VectorClock& vectorClockReceived, ADDRINT processId);
	friend ostream& operator<<(ostream& os, const VectorClock &v);

	void printVector(FILE* out);
};

#endif /* VECTORCLOCK_H_ */
