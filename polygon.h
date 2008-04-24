
#ifndef __POLYGON_H
#define __POLYGON_H

template <class T>
class SList {
public:
    T   *elem;
    int  n;
    int  elemsAllocated;
};

class SEdge {
public:
    Vector a, b;
};

class SEdgeList {
public:
    SList<SEdge>    l;

};

class SContour {
public:
    SList<Vector>   l;
};

class SPolygon {
public:
    SList<SContour> l;
};

class SPolyhedron {
    SList<SPolygon> l;
public:
};

#endif

