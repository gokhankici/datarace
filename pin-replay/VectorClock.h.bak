#ifndef VECTORCLOCK_H_
#define VECTORCLOCK_H_

#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace std;

const int MAX_THREAD_COUNT=96;

typedef unsigned int uint32_t;

const uint32_t NO_ID=-1;

class VectorClock
{
	private:
		size_t *  v;
	public:

		uint32_t threadId;
	 	void tick();
	 	VectorClock();
		VectorClock(uint32_t threadId);
		~VectorClock();
		VectorClock(VectorClock* inClockPtr, uint32_t threadId);
		void toString();
		void receiveAction(VectorClock* vectorClockReceived);
		size_t* getValues() const;
		void sendEvent();
		bool happensBefore(VectorClock* input);
		/*OPERATORS*/
		VectorClock& operator++(); //prefix increment ++vclock
		VectorClock operator++(int x); //postfix increment
		bool operator==(const VectorClock &vRight);
		bool operator!=(const VectorClock &vRight);
		bool operator<(const VectorClock& vRight);
		bool operator<=(const VectorClock& vRight);
		bool areConcurrent(VectorClock* vectorClockReceived, uint32_t threadId);
		friend ostream& operator<<(ostream& os, const VectorClock &v);
};


#endif /* VECTORCLOCK_H_ */
