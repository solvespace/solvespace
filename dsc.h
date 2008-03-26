
#ifndef __DSC_H
#define __DSC_H

typedef unsigned long DWORD;
typedef unsigned char BYTE;

typedef struct {
    double x, y, z;
} Vector;

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
