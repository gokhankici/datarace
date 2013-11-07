// This object is defined using YOOP Specification 1

#ifndef POINT_H
#define POINT_H

#include <math.h>

// Required by YOOP1
#define THIS ((Point *)_this)

// ******************
// *   CLASS BODY   *
// ******************

typedef struct PointBody {
    // Members
    double x, y;
    
    // Methods
    double (*distance)(struct PointBody *);
    void (*delete)();
} Point;

// ******************
// * IMPLEMENTATION *
// ******************

double euclidianDistance(Point * p) {
    double xDist = p->x - THIS->x;
    double yDist = p->y - THIS->y;
    return sqrt(xDist * xDist + yDist * yDist);
}

// Deletes this Point object and frees the memory
void defaultDeletePoint() {
    free(THIS);
}

// Initializes and returns a new Point object
Point * createPoint(double x, double y, double (*distFunction)(Point *)) {
    Point * this = NEW(Point);
    
    this->x = x;
    this->y = y;

    this->distance = distFunction;
    this->delete = defaultDeletePoint;
    
    return this;
}

// Required by YOOP1
#undef THIS

#endif
