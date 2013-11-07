#ifndef /* CLASSNAME_H */
#define /* CLASSNAME_H */

// This object is defined using YOOP Specification 1

// "#include" statements go here

// Required by YOOP1
#define THIS ((/* ClassName */ *)_this)

// ******************
// *   CLASS BODY   *
// ******************

typedef struct /* ClassNameBody */ {
    // Members
    
    // Methods
    void (*delete)();
} /* ClassName */;

// ******************
// * IMPLEMENTATION *
// ******************

// Required by YOOP1
#undef THIS

#endif
