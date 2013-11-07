// This object is defined using YOOP Specification 1

#ifndef STOPWATCH_H
#define STOPWATCH_H

#include "getRealTime.h"

// Required by YOOP1
#define THIS ((StopWatch *)_this)

// ******************
// *   CLASS BODY   *
// ******************

#define NOW getRealTime()

typedef struct StopWatchBody {
    // Members
    double startTime;
    double endTime;
    double pausedTime;
    char state;
    
    // Methods
    void (*start)();
    void (*pause)();
    void (*reset)();
    void (*stop)();
    double (*diff)();
    void (*delete)();
    
    // STATES
    #define STOPWATCH_INITIAL 0
    #define STOPWATCH_WORKING 1
    #define STOPWATCH_PAUSED  2
    #define STOPWATCH_STOPPED 3
} StopWatch;

// ******************
// * IMPLEMENTATION *
// ******************

void startStopWatch() {
    switch (THIS->state) 
    {
        case STOPWATCH_WORKING:
            break;
        case STOPWATCH_INITIAL:
        case STOPWATCH_STOPPED:
            THIS->reset();
            THIS->state = STOPWATCH_WORKING;
            break;
        case STOPWATCH_PAUSED:
            THIS->pausedTime += NOW - THIS->endTime;
            THIS->state = STOPWATCH_WORKING;
            break;
    }
}

void pauseStopWatch() {
    switch (THIS->state)
    {
        case STOPWATCH_WORKING:
            THIS->state = STOPWATCH_PAUSED;
            THIS->endTime = NOW;
            break;
        case STOPWATCH_INITIAL:
        case STOPWATCH_STOPPED:
        case STOPWATCH_PAUSED:
            break;
    }
}

void resetStopWatch() {
    THIS->state = STOPWATCH_INITIAL;
    THIS->startTime = THIS->endTime = NOW;
    THIS->pausedTime = 0.00;
}

void stopStopWatch() {
    switch (THIS->state) {
        case STOPWATCH_PAUSED:
            THIS->start();
            THIS->stop();
            break;
        case STOPWATCH_WORKING:
            THIS->state = STOPWATCH_STOPPED;
            THIS->endTime = NOW;
            break;
        case STOPWATCH_INITIAL:
        case STOPWATCH_STOPPED:
            break;
    }
}

// Returns elapsed time in seconds
double diffStopWatch() {
    return THIS->endTime - THIS->startTime - THIS->pausedTime;
}

void defaultDeleteStopWatch() {
    free(THIS);
}

// Initializes and returns a new Stopwatch object
StopWatch * createStopWatch() {
    StopWatch * this = NEW(StopWatch);

    this->start = startStopWatch;
    this->pause = pauseStopWatch;
    this->reset = resetStopWatch;
    this->stop = stopStopWatch;
    this->diff = diffStopWatch;
    this->delete = defaultDeleteStopWatch;
    
    this->state = STOPWATCH_INITIAL;
    this->startTime = this->endTime = NOW;
    this->pausedTime = 0.00;
    
    return this;
}

// Required by YOOP1
#undef THIS

#endif
