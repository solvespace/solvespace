
#ifndef __SKETCH_H
#define __SKETCH_H

class hGroup;
class hRequest;
class hEntity;
class hParam;
class hPoint;

class Entity;
class Param;
class Point;

class hEquation;
class Equation;

// All of the hWhatever handles are a 32-bit ID, that is used to represent
// some data structure in the sketch.
class hGroup {
public:
    // bits 10: 0   -- group index
    DWORD v;
};
class hRequest {
public:
    // bits 14: 0   -- request index (the high bits may be used as an import ID)
    DWORD   v;

    inline hEntity entity(int i);
};
class hEntity {
public:
    // bits  9: 0   -- entity index
    //      24:10   -- request index
    DWORD   v;

    inline hRequest request(void);
    inline hParam param(int i);
    inline hPoint point(int i);
};
class hParam {
public:
    // bits  6: 0   -- param index
    //      16: 7   -- entity index
    //      31:17   -- request index
    DWORD       v;
};
class hPoint {
    // bits  6: 0   -- point index
    //      16: 7   -- entity index
    //      31:17   -- request index
public:
    DWORD   v;

    inline bool isFromReferences(void);
};

// A set of requests. Every request must have an associated group.
class Group {
public:
    static const hGroup     HGROUP_REFERENCES;

    hGroup      h;

    NameStr     name;

    char *DescriptionString(void);
};

// A user request for some primitive or derived operation; for example a
// line, or a 
class Request {
public:
    // Some predefined requests, that are present in every sketch.
    static const hRequest   HREQUEST_REFERENCE_XY;
    static const hRequest   HREQUEST_REFERENCE_YZ;
    static const hRequest   HREQUEST_REFERENCE_ZX;

    hRequest    h;

    // Types of requests
    static const int CSYS_2D                = 10;
    static const int DATUM_POINT            = 11;
    static const int LINE_SEGMENT           = 20;

    int         type;

    hEntity     csys; // or Entity::NO_CSYS

    hGroup      group;

    NameStr     name;

    inline hEntity entity(int i)
        { hEntity r; r.v = ((this->h.v) << 10) | i; return r; }

    void AddParam(IdList<Param,hParam> *param, Entity *e, int index);
    void Generate(IdList<Entity,hEntity> *entity,
                  IdList<Point,hPoint> *point,
                  IdList<Param,hParam> *param);

    char *DescriptionString(void);
};

class Entity {
public:
    static const hEntity    NO_CSYS;

    static const int CSYS_2D                = 1000;
    static const int DATUM_POINT            = 1001;
    static const int LINE_SEGMENT           = 1010;
    int         type;

    hEntity     h;

    Expr        *expr[16];

    inline hRequest request(void)
        { hRequest r; r.v = (this->h.v >> 10); return r; }
    inline hParam param(int i)
        { hParam r; r.v = ((this->h.v) << 7) | i; return r; }
    inline hPoint point(int i)
        { hPoint r; r.v = ((this->h.v) << 7) | i; return r; }

    char *DescriptionString(void);

    void Get2dCsysBasisVectors(Vector *u, Vector *v);

    struct {
        bool    drawing;
        Point2d mp;
        double  dmin;
    } dogd; // state for drawing or getting distance (for hit testing)
    void LineDrawOrGetDistance(Vector a, Vector b);
    void DrawOrGetDistance(void);
    void Draw(void);
    double GetDistance(Point2d mp);
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
        { hEntity r; r.v = (h.v >> 7); return r; }
    inline hParam param(int i)
        { hParam r; r.v = h.v + i; return r; }

    // The point, in base coordinates. This may be a single parameter, or
    // it may be a more complex expression if our point is locked in a 
    // 2d csys.
    void GetExprs(Expr **x, Expr **y, Expr **z);
    Vector GetCoords(void);

    void ForceTo(Vector v);

    void Draw(void);
    double GetDistance(Point2d mp);
};

class hConstraint {
public:
    DWORD   v;
};

class Constraint {
public:
    static const int USER_EQUATION      = 10;
    static const int POINTS_COINCIDENT  = 20;
    static const int PT_PT_DISTANCE     = 30;
    static const int PT_LINE_DISTANCE   = 31;

    static const int HORIZONTAL         = 40;
    static const int VERTICAL           = 41;

    hConstraint h;
    int         type;
    hGroup      group;

    // These are the parameters for the constraint.
    Expr        *exprA;
    Expr        *exprB;
    hPoint      ptA;
    hPoint      ptB;
    hPoint      ptC;
    hEntity     entityA;
    hEntity     entityB;

    // These define how the constraint is drawn on-screen.
    struct {
        hEntity     csys;
        Vector      offset;
        Vector      u, v;
    } disp;

    static hConstraint AddConstraint(Constraint *c);
    static void MenuConstrain(int id);
    
    struct {
        bool    drawing;
        Point2d mp;
        double  dmin;
    } dogd; // state for drawing or getting distance (for hit testing)
    double GetDistance(Point2d mp);
    void Draw(void);
    void DrawOrGetDistance(void);

    bool HasLabel(void);

    void Generate(IdList<Equation,hEquation> *l);
};

class hEquation {
public:
    DWORD v;
};

class Equation {
public:
    hEquation   h;
    Expr        *e;
};


inline hEntity hRequest::entity(int i)
    { hEntity r; r.v = (v << 10) | i; return r; }

inline hRequest hEntity::request(void)
    { hRequest r; r.v = (v >> 10); return r; }
inline hParam hEntity::param(int i)
    { hParam r; r.v = (v << 7) | i; return r; }
inline hPoint hEntity::point(int i)
    { hPoint r; r.v = (v << 7) | i; return r; }

inline bool hPoint::isFromReferences(void) {
    DWORD d = v >> 17;
    if(d == Request::HREQUEST_REFERENCE_XY.v) return true;
    if(d == Request::HREQUEST_REFERENCE_YZ.v) return true;
    if(d == Request::HREQUEST_REFERENCE_ZX.v) return true;
    return false;
}

#endif
