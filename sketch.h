
#ifndef __SKETCH_H
#define __SKETCH_H

class hEntity;
class Entity;
class hPoint;
class hRequest;
class hParam;
class hGroup;

class hGroup {
public:
    // bits 11: 0   -- group index
    QWORD v;
};

class hRequest {
public:
    // bits 11: 0   -- request index
    QWORD   v;

    hEntity entity(hGroup g, int i);
};

class hParam {
public:
    // bits  7: 0   -- param index
    //      10: 8   -- type (0 for 3d point, 7 for from entity)
    //      15:11   -- point index, or zero if from entity
    //      31:16   -- entity index
    //      43:32   -- request index
    //      55:44   -- group index
    QWORD       v;

    inline hGroup group(void) { hGroup r; r.v = (v >> 44); return r; }
};

class hPoint {
public:
    // bits  2: 0   -- type (0 for 3d point)
    //       7: 3   -- point index
    //      23: 8   -- entity index
    //      35:24   -- request index
    //      47:36   -- group index
    QWORD   v;

    inline hParam param(int i) {
        hParam r;
        r.v = (v << 8) | i;
        return r;
    }
};

class hEntity {
public:
    // bits 15: 0   -- entity index
    //      27:16   -- request index
    //      39:17   -- group index
    QWORD   v;

    inline hGroup group(void)
        { hGroup r; r.v = (v >> 28); return r; }
    inline hRequest request(void)
        { hRequest r; r.v = (v >> 16) & 0xfff; return r; }
    inline hPoint point3(int i)
        { hPoint r; r.v = (v << 8) | (i << 3) | 0; return r; }

    inline hParam param(int i) {
        hParam r;
        r.v = (((v << 8) | 7) << 8) | i;
        return r;
    }
};

// A set of requests. Every request must have an associated group. A group
// may have an associated 2-d coordinate system, in which cases lines or
// curves that belong to the group are automatically constrained into that
// plane; otherwise they are free in 3-space.
class Group {
public:
    static const hGroup     HGROUP_REFERENCES;

    hEntity     csys;
    NameStr     name;
};

// A user request for some primitive or derived operation; for example a
// line, or a 
class Request {
public:
    static const hRequest   HREQUEST_REFERENCE_XY;
    static const hRequest   HREQUEST_REFERENCE_YZ;
    static const hRequest   HREQUEST_REFERENCE_ZX;

    static const int TWO_D_CSYS             = 0;
    static const int LINE_SEGMENT           = 1;

    int         type;

    hRequest    h;

    hGroup      group;
    hEntity     csys;

    NameStr     name;

    void GenerateEntities(IdList<Entity,hEntity> *l, IdList<Group,hGroup> *gl);
};

class Param {
public:
    double      val;

    hParam      h;
};

class Point {
public:
    hPoint      h;

    hEntity     csys;

    void GenerateParams(IdList<Param,hParam> *l);
};

class Entity {
public:
    static const hEntity    NONE;

    static const int TWO_D_CSYS             = 100;
    static const int LINE_SEGMENT           = 101;
    int         type;

    hEntity     h;
    hEntity     csys;

    void Draw(void);
    void GeneratePointsAndParams(IdList<Point,hPoint> *pt, 
                                 IdList<Param,hParam> *pa);
};

// Must be defined later, once hEntity has been defined.
inline hEntity hRequest::entity(hGroup g, int i) {
    hEntity r;
    r.v = (g.v << 28) | (v << 16) | i;
    return r;
}


#endif
