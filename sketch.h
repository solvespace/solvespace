
#ifndef __SKETCH_H
#define __SKETCH_H

class hEntity;
class hPoint;
class hRequest;
class hParam;
class hGroup;

class Entity;

// All of the hWhatever handles are a 32-bit ID, that is used to represent
// some data structure in the sketch.
class hGroup {
public:
    // bits 10: 0   -- group index
    DWORD v;
};
class hRequest {
public:
    // bits 10: 0   -- request index
    DWORD   v;
};
class hEntity {
public:
    // bits 10: 0   -- entity index
    //      21:11   -- request index
    DWORD   v;
};
class hParam {
public:
    // bits  7: 0   -- param index
    //      18: 8   -- entity index
    //      29:19   -- request index
    DWORD       v;
};
class hPoint {
    // bits  7: 0   -- point index
    //      18: 8   -- entity index
    //      29:19   -- request index
public:
    DWORD   v;
};

// A set of requests. Every request must have an associated group. A group
// may have an associated 2-d coordinate system, in which cases lines or
// curves that belong to the group are automatically constrained into that
// plane; otherwise they are free in 3-space.
class Group {
public:
    static const hGroup     HGROUP_REFERENCES;

    hGroup      h;

    hEntity     csys;   // or Entity::NO_CSYS, if it's not locked in a 2d csys
    NameStr     name;
};

// A user request for some primitive or derived operation; for example a
// line, or a 
class Request {
public:
    // Some predefined requests, that are present in every sketch.
    static const hRequest   HREQUEST_REFERENCE_XY;
    static const hRequest   HREQUEST_REFERENCE_YZ;
    static const hRequest   HREQUEST_REFERENCE_ZX;

    // Types of requests
    static const int CSYS_2D                = 0;
    static const int LINE_SEGMENT           = 1;

    int         type;

    hRequest    h;

    hGroup      group;

    NameStr     name;

    inline hEntity entity(int i)
        { hEntity r; r.v = ((this->h.v) << 11) | i; return r; }

    void AddParam(Entity *e, int index);
    void Generate(void);
};

class Entity {
public:
    static const hEntity    NO_CSYS;

    static const int CSYS_2D                = 100;
    static const int LINE_SEGMENT           = 101;
    int         type;

    hEntity     h;

    Expr        *expr[16];

    inline hRequest request(void)
        { hRequest r; r.v = (this->h.v >> 11); return r; }
    inline hParam param(int i)
        { hParam r; r.v = ((this->h.v) << 8) | i; return r; }
    inline hPoint point(int i)
        { hPoint r; r.v = ((this->h.v) << 8) | i; return r; }

    void LineDrawHitTest(Vector a, Vector b);
    void Draw(void);
};

class Param {
public:
    hParam      h;
    double      val;
    bool        known;

    void ForceTo(double v);
};

class Point {
public:
    // The point ID is equal to the initial param ID.
    hPoint      h;

    int type;
    static const int IN_FREE_SPACE  = 0;    // three params, x y z
    static const int IN_2D_CSYS     = 1;    // two params, u v, plus csys
    static const int BY_EXPR        = 2;    // three Expr *, could be anything

    hEntity     csys;

    inline hEntity entity(void)
        { hEntity r; r.v = (h.v >> 8); return r; }
    inline hParam param(int i)
        { hParam r; r.v = h.v + i; return r; }

    // The point, in base coordinates. This may be a single parameter, or
    // it may be a more complex expression if our point is locked in a 
    // 2d csys.
    Expr *x(void);
    Expr *y(void);
    Expr *z(void);

    void ForceTo(Vector v);
    void GetInto(Vector *v);
};

#endif
