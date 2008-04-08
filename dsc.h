
#ifndef __DSC_H
#define __DSC_H

typedef unsigned __int64 QWORD;
typedef unsigned long DWORD;
typedef unsigned char BYTE;

class Vector {
public:
    double x, y, z;

    Vector Plus(Vector b);
    Vector Minus(Vector b);
    Vector Negated(void);
    Vector Cross(Vector b);
    double Dot(Vector b);
    Vector Normal(int which);
    Vector RotatedAbout(Vector axis, double theta);
    double Magnitude(void);
    Vector ScaledBy(double v);

};
void glVertex3v(Vector u);


class Point2d {
public:
    double x, y;
};

template <class T, class H>
class IdList {
public:
    typedef struct {
        T       v;
        H       h;
        int     tag;
    } Elem;

    Elem       *elem;
    int         elems;
    int         elemsAllocated;

    H AddAndAssignId(T *v) {
        H ht;
        ht.v = 0;
        return AddAndAssignId(v, ht);
    }

    H AddAndAssignId(T *v, H ht) {
        int i;
        QWORD id = 0;

        for(i = 0; i < elems; i++) {
            id = max(id, (elem[i].h.v & 0xfff));
        }

        H h;
        h.v = (id + 1) | ht.v;
        AddById(v, h);

        return h;
    }

    void AddById(T *v, H h) {
        if(elems >= elemsAllocated) {
            elemsAllocated = (elemsAllocated + 32)*2;
            elem = (Elem *)realloc(elem, elemsAllocated*sizeof(elem[0]));
            if(!elem) oops();
        }

        elem[elems].v = *v;
        elem[elems].h = h;
        elem[elems].tag = 0;
        elems++;
    }
    
    T *FindById(H h) {
        int i;
        for(i = 0; i < elems; i++) {
            if(elem[i].h.v == h.v) {
                return &(elem[i].v);
            }
        }
        dbp("failed to look up item %16lx, searched %d items", h.v);
        oops();
    }

    void ClearTags(void) {
        int i;
        for(i = 0; i < elems; i++) {
            elem[i].tag = 0;
        }
    }

    void RemoveTagged(void) {
        int src, dest;
        dest = 0;
        for(src = 0; src < elems; src++) {
            if(elem[src].tag) {
                // this item should be deleted
            } else {
                if(src != dest) {
                    elem[dest] = elem[src];
                }
                dest++;
            }
        }
        elems = dest;
        // and elemsAllocated is untouched, because we didn't resize
    }

    void Clear(void) {
        elemsAllocated = elems = 0;
        if(elem) free(elem);
    }

};

class NameStr {
public:
    char str[20];

    inline void strcpy(char *in) {
        memcpy(str, in, min(strlen(in)+1, sizeof(str)));
        str[sizeof(str)-1] = '\0';
    }
};

#endif
