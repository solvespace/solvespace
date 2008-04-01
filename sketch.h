
#ifndef __SKETCH_H
#define __SKETCH_H

class hEntity;
class hPoint;
class hRequest;
class hParam;
class hGroup;

class hRequest {
public:
    int     v;

    hEntity entity(int i);
};

class Request {
public:
    static const hRequest   HREQUEST_DATUM_PLANE_XY,
                            HREQUEST_DATUM_PLANE_YZ,
                            HREQUEST_DATUM_PLANE_ZX;

    static const int FOR_PLANE                  = 0;
    int         type;

    hRequest    h;
};

class hEntity {
public:
    int     v;

    hRequest request(int i);
    hPoint point(int i);
};

class Entity {
public:
    static const int ENTITY_PLANE                               = 0;
    int         type;

    hEntity     h;

    void Draw(void);
};

class hPoint {
public:
    int     v;
};

class Point {
public:
    
    hPoint      h;
};

class hParam {
public:
    int         v;
};

class Param {
public:
    double      val;

    hParam      h;
};

#endif

