
#ifndef __DSC_H
#define __DSC_H

typedef unsigned long DWORD;
typedef unsigned char BYTE;

typedef struct VectorTag Vector;

typedef struct VectorTag {
    double x, y, z;

    Vector Cross(Vector b);
    double Vector::Dot(Vector b);
    Vector RotatedAbout(Vector axis, double theta);
    double Magnitude(void);
    Vector ScaledBy(double v);

} Vector;

typedef struct {
    double x, y;
} Point2d;

template <class T, class H> struct IdList {
    typedef struct {
        T       v;
        int     tag;
    } Elem;

    Elem        elem;
    int         elems;
    int         elemsAllocated;

    void addAndAssignId(T *v);
    void removeTagged(void);

    void clear(void);
};

#endif
