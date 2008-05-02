
#ifndef __SKETCH_H
#define __SKETCH_H

class hGroup;
class hRequest;
class hEntity;
class hParam;

class Entity;
class Param;

class hEquation;
class Equation;

// All of the hWhatever handles are a 32-bit ID, that is used to represent
// some data structure in the sketch.
class hGroup {
public:
    // bits 15: 0   -- group index
    DWORD v;

    inline hEntity entity(int i);
    inline hParam param(int i);
};
class hRequest {
public:
    // bits 15: 0   -- request index
    DWORD   v;

    inline hEntity entity(int i);
    inline hParam param(int i);

    inline bool IsFromReferences(void);
};
class hEntity {
public:
    // bits 15: 0   -- entity index
    //      31:16   -- request index
    DWORD   v;

    inline bool isFromRequest(void);
    inline hRequest request(void);
};
class hParam {
public:
    // bits 15: 0   -- param index
    //      31:16   -- request index
    DWORD       v;

    inline hRequest request(void);
};


class EntityId {
public:
    DWORD v;        // entity ID, starting from 0
};
class EntityMap {
public:
    int         tag;

    EntityId    h;
    hEntity     input;
    int         copyNumber;
    // (input, copyNumber) gets mapped to ((Request)xxx).entity(h.v)
};

// A set of requests. Every request must have an associated group.
class Group {
public:
    static const hGroup     HGROUP_REFERENCES;

    int         tag;
    hGroup      h;

    static const int DRAWING                       = 5000;
    static const int EXTRUDE                       = 5010;
    int type;

    int         solveOrder;
    bool        solved;

    hGroup      opA;
    hGroup      opB;
    bool        visible;

    SEdgeList       edges;
    SList<SPolygon> faces;
    struct {
        SEdge           notClosedAt;
        bool            yes;
    }               polyError;

    NameStr     name;
    char *DescriptionString(void);

    static void AddParam(IdList<Param,hParam> *param, hParam hp, double v);
    void Generate(IdList<Entity,hEntity> *entity, IdList<Param,hParam> *param);
    // When a request generates entities from entities, and the source
    // entities may have come from multiple requests, it's necessary to
    // remap the entity ID so that it's still unique. We do this with a
    // mapping list.
    IdList<EntityMap,EntityId> remap;
    hEntity Remap(hEntity in, int copyNumber);
    void CopyEntity(hEntity in, int a, hParam dx, hParam dy, hParam dz);

    void MakePolygons(void);
    void Draw(void);

    SPolygon GetPolygon(void);

    static void MenuGroup(int id);
};

// A user request for some primitive or derived operation; for example a
// line, or a step and repeat.
class Request {
public:
    // Some predefined requests, that are present in every sketch.
    static const hRequest   HREQUEST_REFERENCE_XY;
    static const hRequest   HREQUEST_REFERENCE_YZ;
    static const hRequest   HREQUEST_REFERENCE_ZX;

    int         tag;
    hRequest    h;

    // Types of requests
    static const int WORKPLANE              = 100;
    static const int DATUM_POINT            = 101;
    static const int LINE_SEGMENT           = 200;
    static const int CUBIC                  = 300;

    int         type;

    hEntity     workplane; // or Entity::FREE_IN_3D
    hGroup      group;

    NameStr     name;
    bool        construction;
    
    static hParam AddParam(IdList<Param,hParam> *param, hParam hp);
    void Generate(IdList<Entity,hEntity> *entity, IdList<Param,hParam> *param);

    char *DescriptionString(void);
};

class Entity {
public:
    int         tag;
    hEntity     h;

    static const hEntity    FREE_IN_3D;

    static const int WORKPLANE              =  1000;
    static const int POINT_IN_3D            =  2000;
    static const int POINT_IN_2D            =  2001;
    static const int POINT_XFRMD            =  2010;
    static const int LINE_SEGMENT           = 10000;
    static const int CUBIC                  = 11000;

    static const int EDGE_LIST              = 90000;
    static const int FACE_LIST              = 91000;
    int         type;

    // When it comes time to draw an entity, we look here to get the
    // defining variables.
    hParam      param[4];
    hEntity     point[4];
    hEntity     direction;

    hGroup      group;
    hEntity     workplane;   // or Entity::FREE_IN_3D

    // For entities that are derived by a transformation, the number of
    // times to apply the transformation.
    int timesApplied;

    // Applies only for a WORKPLANE type
    void WorkplaneGetBasisVectors(Vector *u, Vector *v);
    Vector WorkplaneGetNormalVector(void);
    void WorkplaneGetBasisExprs(ExprVector *u, ExprVector *v);
    ExprVector WorkplaneGetOffsetExprs(void);

    bool IsPoint(void);
    // Applies for any of the point types
    Vector PointGetCoords(void);
    ExprVector PointGetExprs(void);
    void PointGetExprsInWorkplane(hEntity wrkpl, Expr **u, Expr **v);
    void PointForceTo(Vector v);
    bool PointIsFromReferences(void);

    // Applies for anything that comes with a plane
    bool HasPlane(void);
    // The plane is points P such that P dot (xn, yn, zn) - d = 0
    void PlaneGetExprs(ExprVector *n, Expr **d);

    // Routines to draw and hit-test the representation of the entity
    // on-screen.
    struct {
        bool        drawing;
        Point2d     mp;
        double      dmin;
        SEdgeList   *edges;
    } dogd; // state for drawing or getting distance (for hit testing)
    void LineDrawOrGetDistance(Vector a, Vector b);
    void LineDrawOrGetDistanceOrEdge(Vector a, Vector b);
    void DrawOrGetDistance(int order);

    void Draw(int order);
    double GetDistance(Point2d mp);
    void GenerateEdges(SEdgeList *el);

    char *DescriptionString(void);
};

class Param {
public:
    int         tag;
    hParam      h;

    double      val;
    bool        known;
    bool        assumed;
};


inline hEntity hGroup::entity(int i)
    { hEntity r; r.v = 0x80000000 | (v << 16) | i; return r; }
inline hParam hGroup::param(int i)
    { hParam r; r.v = 0x80000000 | (v << 16) | i; return r; }

inline bool hRequest::IsFromReferences(void) {
    if(v == Request::HREQUEST_REFERENCE_XY.v) return true;
    if(v == Request::HREQUEST_REFERENCE_YZ.v) return true;
    if(v == Request::HREQUEST_REFERENCE_ZX.v) return true;
    return false;
}
inline hEntity hRequest::entity(int i)
    { hEntity r; r.v = (v << 16) | i; return r; }
inline hParam hRequest::param(int i)
    { hParam r; r.v = (v << 16) | i; return r; }

inline bool hEntity::isFromRequest(void)
    { if(v & 0x80000000) return false; else return true; }
inline hRequest hEntity::request(void)
    { hRequest r; r.v = (v >> 16); return r; }

inline hRequest hParam::request(void)
    { hRequest r; r.v = (v >> 16); return r; }


class hConstraint {
public:
    DWORD   v;

    hEquation equation(int i);
};

class Constraint {
public:
    static const int USER_EQUATION      = 10;
    static const int POINTS_COINCIDENT  = 20;
    static const int PT_PT_DISTANCE     = 30;
    static const int PT_LINE_DISTANCE   = 31;
    static const int PT_IN_PLANE        = 40;
    static const int PT_ON_LINE         = 41;
    static const int EQUAL_LENGTH_LINES = 50;
    static const int SYMMETRIC          = 60;
    static const int AT_MIDPOINT        = 70;

    static const int HORIZONTAL         = 80;
    static const int VERTICAL           = 81;

    int         tag;
    hConstraint h;

    int         type;

    hGroup      group;
    hEntity     workplane;

    // These are the parameters for the constraint.
    Expr        *exprA;
    Expr        *exprB;
    hEntity     ptA;
    hEntity     ptB;
    hEntity     ptC;
    hEntity     entityA;
    hEntity     entityB;

    // These define how the constraint is drawn on-screen.
    struct {
        Vector      offset;
    } disp;

    char *DescriptionString(void);

    static hConstraint AddConstraint(Constraint *c);
    static void MenuConstrain(int id);
    
    struct {
        bool        drawing;
        Point2d     mp;
        double      dmin;
    } dogd; // state for drawing or getting distance (for hit testing)
    void LineDrawOrGetDistance(Vector a, Vector b);
    void DrawOrGetDistance(Vector *labelPos);

    double GetDistance(Point2d mp);
    Vector GetLabelPos(void);
    void Draw(void);

    bool HasLabel(void);

    void Generate(IdList<Equation,hEquation> *l);
    // Some helpers when generating symbolic constraint equations
    void ModifyToSatisfy(void);
    void AddEq(IdList<Equation,hEquation> *l, Expr *expr, int index);
    static Expr *Distance(hEntity workplane, hEntity pa, hEntity pb);
    static Expr *PointLineDistance(hEntity workplane, hEntity pt, hEntity ln);
    static Expr *PointPlaneDistance(ExprVector p, hEntity plane);
    static Expr *VectorsParallel(int eq, ExprVector a, ExprVector b);
    static ExprVector PointInThreeSpace(hEntity workplane, Expr *u, Expr *v);

    static void ConstrainCoincident(hEntity ptA, hEntity ptB);
};

class hEquation {
public:
    DWORD v;
};

class Equation {
public:
    int         tag;
    hEquation   h;

    Expr        *e;
};

inline hEquation hConstraint::equation(int i)
    { hEquation r; r.v = (v << 16) | i; return r; }


#endif
