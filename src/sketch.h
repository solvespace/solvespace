//-----------------------------------------------------------------------------
// The parametric structure of our sketch, in multiple groups, that generate
// geometric entities and surfaces.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------

#ifndef __SKETCH_H
#define __SKETCH_H

class hGroup;
class hRequest;
class hEntity;
class hParam;
class hStyle;
class hConstraint;
class hEquation;

class Entity;
class Param;
class Equation;


// All of the hWhatever handles are a 32-bit ID, that is used to represent
// some data structure in the sketch.
class hGroup {
public:
    // bits 15: 0   -- group index
    uint32_t v;

    inline hEntity entity(int i);
    inline hParam param(int i);
    inline hEquation equation(int i);
};
class hRequest {
public:
    // bits 15: 0   -- request index
    uint32_t v;

    inline hEntity entity(int i);
    inline hParam param(int i);

    inline bool IsFromReferences(void);
};
class hEntity {
public:
    // bits 15: 0   -- entity index
    //      31:16   -- request index
    uint32_t v;

    inline bool isFromRequest(void);
    inline hRequest request(void);
    inline hGroup group(void);
    inline hEquation equation(int i);
};
class hParam {
public:
    // bits 15: 0   -- param index
    //      31:16   -- request index
    uint32_t v;

    inline hRequest request(void);
};

class hStyle {
public:
    uint32_t v;
};


class EntityId {
public:
    uint32_t v;     // entity ID, starting from 0
};
class EntityMap {
public:
    int         tag;

    EntityId    h;
    hEntity     input;
    int         copyNumber;
    // (input, copyNumber) gets mapped to ((Request)xxx).entity(h.v)

    void Clear(void) {}
};

// A set of requests. Every request must have an associated group.
class Group {
public:
    static const hGroup     HGROUP_REFERENCES;

    int         tag;
    hGroup      h;

    enum {
        DRAWING_3D                    = 5000,
        DRAWING_WORKPLANE             = 5001,
        EXTRUDE                       = 5100,
        LATHE                         = 5101,
        ROTATE                        = 5200,
        TRANSLATE                     = 5201,
        IMPORTED                      = 5300
    };
    int type;

    int order;

    hGroup      opA;
    hGroup      opB;
    bool        visible;
    bool        suppress;
    bool        relaxConstraints;
    bool        allDimsReference;
    double      scale;

    bool        clean;
    hEntity     activeWorkplane;
    double      valA;
    double      valB;
    double      valC;
    RgbaColor   color;

    struct {
        int                 how;
        int                 dof;
        List<hConstraint>   remove;
    } solved;

    enum {
        // For drawings in 2d
        WORKPLANE_BY_POINT_ORTHO   = 6000,
        WORKPLANE_BY_LINE_SEGMENTS = 6001,
        // For extrudes, translates, and rotates
        ONE_SIDED                  = 7000,
        TWO_SIDED                  = 7001
    };
    int subtype;

    bool skipFirst; // for step and repeat ops

    struct {
        Quaternion  q;
        hEntity     origin;
        hEntity     entityB;
        hEntity     entityC;
        bool        swapUV;
        bool        negateU;
        bool        negateV;
    } predef;

    SPolygon                polyLoops;
    SBezierLoopSetSet       bezierLoops;
    SBezierList             bezierOpens;
    enum {
        POLY_GOOD              = 0,
        POLY_NOT_CLOSED        = 1,
        POLY_NOT_COPLANAR      = 2,
        POLY_SELF_INTERSECTING = 3,
        POLY_ZERO_LEN_EDGE     = 4
    };
    struct {
        int             how;
        SEdge           notClosedAt;
        Vector          errorPointAt;
    }               polyError;

    bool            booleanFailed;

    SShell          thisShell;
    SShell          runningShell;

    SMesh           thisMesh;
    SMesh           runningMesh;

    bool            displayDirty;
    SMesh           displayMesh;
    SEdgeList       displayEdges;

    enum {
        COMBINE_AS_UNION           = 0,
        COMBINE_AS_DIFFERENCE      = 1,
        COMBINE_AS_ASSEMBLE        = 2
    };
    int meshCombine;

    bool forceToMesh;

    IdList<EntityMap,EntityId> remap;
    enum { REMAP_PRIME = 19477 };
    int remapCache[REMAP_PRIME];

    std::string impFile;
    std::string impFileRel;
    SMesh       impMesh;
    SShell      impShell;
    EntityList  impEntity;

    std::string     name;


    void Activate(void);
    std::string DescriptionString(void);
    void Clear(void);

    static void AddParam(ParamList *param, hParam hp, double v);
    void Generate(EntityList *entity, ParamList *param);
    void TransformImportedBy(Vector t, Quaternion q);
    // When a request generates entities from entities, and the source
    // entities may have come from multiple requests, it's necessary to
    // remap the entity ID so that it's still unique. We do this with a
    // mapping list.
    enum {
        REMAP_LAST         = 1000,
        REMAP_TOP          = 1001,
        REMAP_BOTTOM       = 1002,
        REMAP_PT_TO_LINE   = 1003,
        REMAP_LINE_TO_FACE = 1004,
        REMAP_LATHE_START  = 1006,
        REMAP_LATHE_END    = 1007,
        REMAP_PT_TO_ARC    = 1008,
        REMAP_PT_TO_NORMAL = 1009,
    };
    hEntity Remap(hEntity in, int copyNumber);
    void MakeExtrusionLines(EntityList *el, hEntity in);
    void MakeLatheCircles(IdList<Entity,hEntity> *el, IdList<Param,hParam> *param, hEntity in, Vector pt, Vector axis, int ai);
    void MakeExtrusionTopBottomFaces(EntityList *el, hEntity pt);
    void CopyEntity(EntityList *el,
                    Entity *ep, int timesApplied, int remap,
                    hParam dx, hParam dy, hParam dz,
                    hParam qw, hParam qvx, hParam qvy, hParam qvz,
                    bool asTrans, bool asAxisAngle);

    void AddEq(IdList<Equation,hEquation> *l, Expr *expr, int index);
    void GenerateEquations(IdList<Equation,hEquation> *l);
    bool IsVisible(void);

    // Assembling the curves into loops, and into a piecewise linear polygon
    // at the same time.
    void AssembleLoops(bool *allClosed, bool *allCoplanar, bool *allNonZeroLen);
    void GenerateLoops(void);
    // And the mesh stuff
    Group *PreviousGroup(void);
    Group *RunningMeshGroup(void);
    void GenerateShellAndMesh(void);
    template<class T> void GenerateForStepAndRepeat(T *steps, T *outs);
    template<class T> void GenerateForBoolean(T *a, T *b, T *o, int how);
    void GenerateDisplayItems(void);
    void DrawDisplayItems(int t);
    void Draw(void);
    RgbaColor GetLoopSetFillColor(SBezierLoopSet *sbls,
                                 bool *allSame, Vector *errorAt);
    void FillLoopSetAsPolygon(SBezierLoopSet *sbls);
    void DrawFilledPaths(void);

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
    enum {
        WORKPLANE              = 100,
        DATUM_POINT            = 101,
        LINE_SEGMENT           = 200,
        CUBIC                  = 300,
        CUBIC_PERIODIC         = 301,
        CIRCLE                 = 400,
        ARC_OF_CIRCLE          = 500,
        TTF_TEXT               = 600
    };

    int         type;
    int         extraPoints;

    hEntity     workplane; // or Entity::FREE_IN_3D
    hGroup      group;
    hStyle      style;

    bool        construction;
    std::string str;
    std::string font;

    static hParam AddParam(ParamList *param, hParam hp);
    void Generate(EntityList *entity, ParamList *param);

    std::string DescriptionString(void);

    void Clear(void) {}
};

#define MAX_POINTS_IN_ENTITY (12)
class EntityBase {
public:
    int         tag;
    hEntity     h;

    static const hEntity    FREE_IN_3D;
    static const hEntity    NO_ENTITY;

    enum {
        POINT_IN_3D            =  2000,
        POINT_IN_2D            =  2001,
        POINT_N_TRANS          =  2010,
        POINT_N_ROT_TRANS      =  2011,
        POINT_N_COPY           =  2012,
        POINT_N_ROT_AA         =  2013,

        NORMAL_IN_3D           =  3000,
        NORMAL_IN_2D           =  3001,
        NORMAL_N_COPY          =  3010,
        NORMAL_N_ROT           =  3011,
        NORMAL_N_ROT_AA        =  3012,

        DISTANCE               =  4000,
        DISTANCE_N_COPY        =  4001,

        FACE_NORMAL_PT         =  5000,
        FACE_XPROD             =  5001,
        FACE_N_ROT_TRANS       =  5002,
        FACE_N_TRANS           =  5003,
        FACE_N_ROT_AA          =  5004,


        WORKPLANE              = 10000,
        LINE_SEGMENT           = 11000,
        CUBIC                  = 12000,
        CUBIC_PERIODIC         = 12001,
        CIRCLE                 = 13000,
        ARC_OF_CIRCLE          = 14000,
        TTF_TEXT               = 15000
    };

    int         type;

    hGroup      group;
    hEntity     workplane;   // or Entity::FREE_IN_3D

    // When it comes time to draw an entity, we look here to get the
    // defining variables.
    hEntity     point[MAX_POINTS_IN_ENTITY];
    int         extraPoints;
    hEntity     normal;
    hEntity     distance;
    // The only types that have their own params are points, normals,
    // and directions.
    hParam      param[7];

    // Transformed points/normals/distances have their numerical base
    Vector      numPoint;
    Quaternion  numNormal;
    double      numDistance;

    std::string str;
    std::string font;

    // For entities that are derived by a transformation, the number of
    // times to apply the transformation.
    int timesApplied;

    Quaternion GetAxisAngleQuaternion(int param0);
    ExprQuaternion GetAxisAngleQuaternionExprs(int param0);

    bool IsCircle(void);
    Expr *CircleGetRadiusExpr(void);
    double CircleGetRadiusNum(void);
    void ArcGetAngles(double *thetaa, double *thetab, double *dtheta);

    bool HasVector(void);
    ExprVector VectorGetExprs(void);
    Vector VectorGetNum(void);
    Vector VectorGetRefPoint(void);

    // For distances
    bool IsDistance(void);
    double DistanceGetNum(void);
    Expr *DistanceGetExpr(void);
    void DistanceForceTo(double v);

    bool IsWorkplane(void);
    // The plane is points P such that P dot (xn, yn, zn) - d = 0
    void WorkplaneGetPlaneExprs(ExprVector *n, Expr **d);
    ExprVector WorkplaneGetOffsetExprs(void);
    Vector WorkplaneGetOffset(void);
    EntityBase *Normal(void);

    bool IsFace(void);
    ExprVector FaceGetNormalExprs(void);
    Vector FaceGetNormalNum(void);
    ExprVector FaceGetPointExprs(void);
    Vector FaceGetPointNum(void);

    bool IsPoint(void);
    // Applies for any of the point types
    Vector PointGetNum(void);
    ExprVector PointGetExprs(void);
    void PointGetExprsInWorkplane(hEntity wrkpl, Expr **u, Expr **v);
    void PointForceTo(Vector v);
    // These apply only the POINT_N_ROT_TRANS, which has an assoc rotation
    Quaternion PointGetQuaternion(void);
    void PointForceQuaternionTo(Quaternion q);

    bool IsNormal(void);
    // Applies for any of the normal types
    Quaternion NormalGetNum(void);
    ExprQuaternion NormalGetExprs(void);
    void NormalForceTo(Quaternion q);

    Vector NormalU(void);
    Vector NormalV(void);
    Vector NormalN(void);
    ExprVector NormalExprsU(void);
    ExprVector NormalExprsV(void);
    ExprVector NormalExprsN(void);

    Vector CubicGetStartNum(void);
    Vector CubicGetFinishNum(void);
    ExprVector CubicGetStartTangentExprs(void);
    ExprVector CubicGetFinishTangentExprs(void);
    Vector CubicGetStartTangentNum(void);
    Vector CubicGetFinishTangentNum(void);

    bool HasEndpoints(void);
    Vector EndpointStart();
    Vector EndpointFinish();

    void AddEq(IdList<Equation,hEquation> *l, Expr *expr, int index);
    void GenerateEquations(IdList<Equation,hEquation> *l);

    void Clear(void) {}
};

class Entity : public EntityBase {
public:
    // Necessary for Entity e = {} to zero-initialize, since
    // classes with base classes are not aggregates and
    // the default constructor does not initialize members.
    //
    // Note EntityBase({}); without explicitly value-initializing
    // the base class, MSVC2013 will default-initialize it, leaving
    // POD members with indeterminate value.
    Entity() : EntityBase({}), forceHidden(), actPoint(), actNormal(),
        actDistance(), actVisible(), style(), construction(),
        dogd() {};

    // An imported entity that was hidden in the source file ends up hidden
    // here too.
    bool        forceHidden;

    // All points/normals/distances have their numerical value; this is
    // a convenience, to simplify the import/assembly code, so that the
    // part is entirely described by the entities.
    Vector      actPoint;
    Quaternion  actNormal;
    double      actDistance;
    // and the shown state also gets saved here, for later import
    bool        actVisible;

    hStyle      style;
    bool        construction;

    // Routines to draw and hit-test the representation of the entity
    // on-screen.
    struct {
        bool        drawing;
        Point2d     mp;
        double      dmin;
        Vector      refp;
        double      lineWidth;
    } dogd; // state for drawing or getting distance (for hit testing)
    void LineDrawOrGetDistance(Vector a, Vector b, bool maybeFat=false);
    void DrawOrGetDistance(void);

    bool IsVisible(void);
    bool PointIsFromReferences(void);

    void ComputeInterpolatingSpline(SBezierList *sbl, bool periodic);
    void GenerateBezierCurves(SBezierList *sbl);
    void GenerateEdges(SEdgeList *el, bool includingConstruction=false);

    static void DrawAll(void);
    void Draw(void);
    double GetDistance(Point2d mp);
    Vector GetReferencePos(void);

    void CalculateNumerical(bool forExport);

    std::string DescriptionString(void);
};

class EntReqTable {
public:
    typedef struct {
        int         reqType;
        int         entType;
        int         points;
        bool        useExtraPoints;
        bool        hasNormal;
        bool        hasDistance;
        const char *description;
    } TableEntry;

    static const TableEntry Table[];

    static const char *DescriptionForRequest(int req);
    static void CopyEntityInfo(const TableEntry *te, int extraPoints,
            int *ent, int *req, int *pts, bool *hasNormal, bool *hasDistance);
    static bool GetRequestInfo(int req, int extraPoints,
                    int *ent, int *pts, bool *hasNormal, bool *hasDistance);
    static bool GetEntityInfo(int ent, int extraPoints,
                    int *req, int *pts, bool *hasNormal, bool *hasDistance);
    static int GetRequestForEntity(int ent);
};

class Param {
public:
    int         tag;
    hParam      h;

    double      val;
    bool        known;
    bool        free;

    // Used only in the solver
    hParam      substd;

    static const hParam NO_PARAM;

    void Clear(void) {}
};


class hConstraint {
public:
    uint32_t v;

    inline hEquation equation(int i);
};

class ConstraintBase {
public:
    int         tag;
    hConstraint h;

    static const hConstraint NO_CONSTRAINT;

    enum {
        POINTS_COINCIDENT      =  20,
        PT_PT_DISTANCE         =  30,
        PT_PLANE_DISTANCE      =  31,
        PT_LINE_DISTANCE       =  32,
        PT_FACE_DISTANCE       =  33,
        PROJ_PT_DISTANCE       =  34,
        PT_IN_PLANE            =  41,
        PT_ON_LINE             =  42,
        PT_ON_FACE             =  43,
        EQUAL_LENGTH_LINES     =  50,
        LENGTH_RATIO           =  51,
        EQ_LEN_PT_LINE_D       =  52,
        EQ_PT_LN_DISTANCES     =  53,
        EQUAL_ANGLE            =  54,
        EQUAL_LINE_ARC_LEN     =  55,
        LENGTH_DIFFERENCE      =  56,
        SYMMETRIC              =  60,
        SYMMETRIC_HORIZ        =  61,
        SYMMETRIC_VERT         =  62,
        SYMMETRIC_LINE         =  63,
        AT_MIDPOINT            =  70,
        HORIZONTAL             =  80,
        VERTICAL               =  81,
        DIAMETER               =  90,
        PT_ON_CIRCLE           = 100,
        SAME_ORIENTATION       = 110,
        ANGLE                  = 120,
        PARALLEL               = 121,
        PERPENDICULAR          = 122,
        ARC_LINE_TANGENT       = 123,
        CUBIC_LINE_TANGENT     = 124,
        CURVE_CURVE_TANGENT    = 125,
        EQUAL_RADIUS           = 130,
        WHERE_DRAGGED          = 200,

        COMMENT                = 1000
    };

    int         type;

    hGroup      group;
    hEntity     workplane;

    // These are the parameters for the constraint.
    double      valA;
    hEntity     ptA;
    hEntity     ptB;
    hEntity     entityA;
    hEntity     entityB;
    hEntity     entityC;
    hEntity     entityD;
    bool        other;
    bool        other2;

    bool        reference;  // a ref dimension, that generates no eqs
    std::string comment;    // since comments are represented as constraints

    bool HasLabel(void);

    void Generate(IdList<Equation,hEquation> *l);
    void GenerateReal(IdList<Equation,hEquation> *l);
    // Some helpers when generating symbolic constraint equations
    void ModifyToSatisfy(void);
    void AddEq(IdList<Equation,hEquation> *l, Expr *expr, int index);
    static Expr *DirectionCosine(hEntity wrkpl, ExprVector ae, ExprVector be);
    static Expr *Distance(hEntity workplane, hEntity pa, hEntity pb);
    static Expr *PointLineDistance(hEntity workplane, hEntity pt, hEntity ln);
    static Expr *PointPlaneDistance(ExprVector p, hEntity plane);
    static Expr *VectorsParallel(int eq, ExprVector a, ExprVector b);
    static ExprVector PointInThreeSpace(hEntity workplane, Expr *u, Expr *v);

    void Clear(void) {}
};

class Constraint : public ConstraintBase {
public:
    // See Entity::Entity().
    Constraint() : ConstraintBase({}), disp(), dogd() {}

    // These define how the constraint is drawn on-screen.
    struct {
        Vector      offset;
        hStyle      style;
    } disp;

    // State for drawing or getting distance (for hit testing).
    struct {
        bool        drawing;
        Point2d     mp;
        double      dmin;
        Vector      refp;
        SEdgeList   *sel;
    } dogd;

    double GetDistance(Point2d mp);
    Vector GetLabelPos(void);
    Vector GetReferencePos(void);
    void Draw(void);
    void GetEdges(SEdgeList *sel);

    void LineDrawOrGetDistance(Vector a, Vector b);
    void DrawOrGetDistance(Vector *labelPos);
    double EllipticalInterpolation(double rx, double ry, double theta);
    std::string Label(void);
    void DoArcForAngle(Vector a0, Vector da, Vector b0, Vector db,
                        Vector offset, Vector *ref);
    void DoLineWithArrows(Vector ref, Vector a, Vector b, bool onlyOneExt);
    int DoLineTrimmedAgainstBox(Vector ref, Vector a, Vector b);
    void DoLabel(Vector ref, Vector *labelPos, Vector gr, Vector gu);
    void StippledLine(Vector a, Vector b);
    void DoProjectedPoint(Vector *p);
    void DoEqualLenTicks(Vector a, Vector b, Vector gn);
    void DoEqualRadiusTicks(hEntity he);

    std::string DescriptionString(void);

    static void AddConstraint(Constraint *c, bool rememberForUndo);
    static void AddConstraint(Constraint *c);
    static void MenuConstrain(int id);
    static void DeleteAllConstraintsFor(int type, hEntity entityA, hEntity ptA);

    static void ConstrainCoincident(hEntity ptA, hEntity ptB);
    static void Constrain(int type, hEntity ptA, hEntity ptB, hEntity entityA);
    static void Constrain(int type, hEntity ptA, hEntity ptB,
                                    hEntity entityA, hEntity entityB,
                                    bool other, bool other2);
};

class hEquation {
public:
    uint32_t v;

    inline bool isFromConstraint(void);
    inline hConstraint constraint(void);
};

class Equation {
public:
    int         tag;
    hEquation   h;

    Expr        *e;

    void Clear(void) {}
};


class Style {
public:
    int         tag;
    hStyle      h;

    enum {
        // If an entity has no style, then it will be colored according to
        // whether the group that it's in is active or not, whether it's
        // construction or not, and so on.
        NO_STYLE       = 0,

        ACTIVE_GRP     = 1,
        CONSTRUCTION   = 2,
        INACTIVE_GRP   = 3,
        DATUM          = 4,
        SOLID_EDGE     = 5,
        CONSTRAINT     = 6,
        SELECTED       = 7,
        HOVERED        = 8,
        CONTOUR_FILL   = 9,
        NORMALS        = 10,
        ANALYZE        = 11,
        DRAW_ERROR     = 12,
        DIM_SOLID      = 13,

        FIRST_CUSTOM   = 0x100
    };

    std::string name;

    enum {
        UNITS_AS_PIXELS   = 0,
        UNITS_AS_MM       = 1
    };
    double      width;
    int         widthAs;
    double      textHeight;
    int         textHeightAs;
    enum {
        ORIGIN_LEFT       = 0x01,
        ORIGIN_RIGHT      = 0x02,
        ORIGIN_BOT        = 0x04,
        ORIGIN_TOP        = 0x08
    };
    int         textOrigin;
    double      textAngle;
    RgbaColor   color;
    bool        filled;
    RgbaColor   fillColor;
    bool        visible;
    bool        exportable;

    // The default styles, for entities that don't have a style assigned yet,
    // and for datums and such.
    typedef struct {
        hStyle      h;
        const char *cnfPrefix;
        RgbaColor   color;
        double      width;
    } Default;
    static const Default Defaults[];

    static std::string CnfColor(const std::string &prefix);
    static std::string CnfWidth(const std::string &prefix);
    static std::string CnfPrefixToName(const std::string &prefix);

    static void CreateAllDefaultStyles(void);
    static void CreateDefaultStyle(hStyle h);
    static void FreezeDefaultStyles(void);
    static void LoadFactoryDefaults(void);

    static void AssignSelectionToStyle(uint32_t v);
    static uint32_t CreateCustomStyle(void);

    static RgbaColor RewriteColor(RgbaColor rgb);

    static Style *Get(hStyle hs);
    static RgbaColor Color(hStyle hs, bool forExport=false);
    static RgbaColor FillColor(hStyle hs, bool forExport=false);
    static float Width(hStyle hs);
    static RgbaColor Color(int hs, bool forExport=false);
    static float Width(int hs);
    static double WidthMm(int hs);
    static double TextHeight(hStyle hs);
    static bool Exportable(int hs);
    static hStyle ForEntity(hEntity he);

    std::string DescriptionString(void);

    void Clear(void) {}
};


inline hEntity hGroup::entity(int i)
    { hEntity r; r.v = 0x80000000 | (v << 16) | (uint32_t)i; return r; }
inline hParam hGroup::param(int i)
    { hParam r; r.v = 0x80000000 | (v << 16) | (uint32_t)i; return r; }
inline hEquation hGroup::equation(int i)
    { hEquation r; r.v = (v << 16) | 0x80000000 | (uint32_t)i; return r; }

inline bool hRequest::IsFromReferences(void) {
    if(v == Request::HREQUEST_REFERENCE_XY.v) return true;
    if(v == Request::HREQUEST_REFERENCE_YZ.v) return true;
    if(v == Request::HREQUEST_REFERENCE_ZX.v) return true;
    return false;
}
inline hEntity hRequest::entity(int i)
    { hEntity r; r.v = (v << 16) | (uint32_t)i; return r; }
inline hParam hRequest::param(int i)
    { hParam r; r.v = (v << 16) | (uint32_t)i; return r; }

inline bool hEntity::isFromRequest(void)
    { if(v & 0x80000000) return false; else return true; }
inline hRequest hEntity::request(void)
    { hRequest r; r.v = (v >> 16); return r; }
inline hGroup hEntity::group(void)
    { hGroup r; r.v = (v >> 16) & 0x3fff; return r; }
inline hEquation hEntity::equation(int i)
    { if(i != 0) oops(); hEquation r; r.v = v | 0x40000000; return r; }

inline hRequest hParam::request(void)
    { hRequest r; r.v = (v >> 16); return r; }


inline hEquation hConstraint::equation(int i)
    { hEquation r; r.v = (v << 16) | (uint32_t)i; return r; }

inline bool hEquation::isFromConstraint(void)
    { if(v & 0xc0000000) return false; else return true; }
inline hConstraint hEquation::constraint(void)
    { hConstraint r; r.v = (v >> 16); return r; }

// The format for entities stored on the clipboard.
class ClipboardRequest {
public:
    int         type;
    int         extraPoints;
    hStyle      style;
    std::string str;
    std::string font;
    bool        construction;

    Vector      point[MAX_POINTS_IN_ENTITY];
    double      distance;

    hEntity     oldEnt;
    hEntity     oldPointEnt[MAX_POINTS_IN_ENTITY];
    hRequest    newReq;
};

#endif
