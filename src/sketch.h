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
    uint32_t   v;

    inline hEntity entity(int i);
    inline hParam param(int i);

    inline bool IsFromReferences(void);
};
class hEntity {
public:
    // bits 15: 0   -- entity index
    //      31:16   -- request index
    uint32_t   v;

    inline bool isFromRequest(void);
    inline hRequest request(void);
    inline hGroup group(void);
    inline hEquation equation(int i);
};
class hParam {
public:
    // bits 15: 0   -- param index
    //      31:16   -- request index
    uint32_t       v;

    inline hRequest request(void);
};

class hStyle {
public:
    uint32_t       v;
};


class EntityId {
public:
    uint32_t v;        // entity ID, starting from 0
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

    static const int DRAWING_3D                    = 5000;
    static const int DRAWING_WORKPLANE             = 5001;
    static const int EXTRUDE                       = 5100;
    static const int LATHE                         = 5101;
    static const int ROTATE                        = 5200;
    static const int TRANSLATE                     = 5201;
    static const int IMPORTED                      = 5300;
    int type;

    int order;

    hGroup      opA;
    hGroup      opB;
    bool        visible;
    bool        suppress;
    bool        relaxConstraints;
    bool        allowRedundant;
    bool        allDimsReference;
    double      scale;

    bool        clean;
    hEntity     activeWorkplane;
    double      valA;
    double      valB;
    double      valC;
    uint32_t       color;

    struct {
        int                 how;
        int                 dof;
        List<hConstraint>   remove;
    } solved;

    // For drawings in 2d
    static const int WORKPLANE_BY_POINT_ORTHO   = 6000;
    static const int WORKPLANE_BY_LINE_SEGMENTS = 6001;
    // For extrudes, translates, and rotates
    static const int ONE_SIDED                  = 7000;
    static const int TWO_SIDED                  = 7001;
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
    static const int POLY_GOOD              = 0;
    static const int POLY_NOT_CLOSED        = 1;
    static const int POLY_NOT_COPLANAR      = 2;
    static const int POLY_SELF_INTERSECTING = 3;
    static const int POLY_ZERO_LEN_EDGE     = 4;
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

    static const int COMBINE_AS_UNION           = 0;
    static const int COMBINE_AS_DIFFERENCE      = 1;
    static const int COMBINE_AS_ASSEMBLE        = 2;
    int meshCombine;

    bool forceToMesh;

    IdList<EntityMap,EntityId> remap;
    static const int REMAP_PRIME = 19477;
    int remapCache[REMAP_PRIME];

    char                       impFile[MAX_PATH];
    char                       impFileRel[MAX_PATH];
    SMesh                      impMesh;
    SShell                     impShell;
    EntityList                 impEntity;

    NameStr     name;


    void Activate(void);
    char *DescriptionString(void);
    void Clear(void);

    static void AddParam(ParamList *param, hParam hp, double v);
    void Generate(EntityList *entity, ParamList *param);
    bool IsSolvedOkay();
    void TransformImportedBy(Vector t, Quaternion q);
    // When a request generates entities from entities, and the source
    // entities may have come from multiple requests, it's necessary to
    // remap the entity ID so that it's still unique. We do this with a
    // mapping list.
    static const int REMAP_LAST         = 1000;
    static const int REMAP_TOP          = 1001;
    static const int REMAP_BOTTOM       = 1002;
    static const int REMAP_PT_TO_LINE   = 1003;
    static const int REMAP_LINE_TO_FACE = 1004;
    hEntity Remap(hEntity in, int copyNumber);
    void MakeExtrusionLines(EntityList *el, hEntity in);
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
    uint32_t GetLoopSetFillColor(SBezierLoopSet *sbls,
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
    static const int WORKPLANE              = 100;
    static const int DATUM_POINT            = 101;
    static const int LINE_SEGMENT           = 200;
    static const int CUBIC                  = 300;
    static const int CUBIC_PERIODIC         = 301;
    static const int CIRCLE                 = 400;
    static const int ARC_OF_CIRCLE          = 500;
    static const int TTF_TEXT               = 600;

    int         type;
    int         extraPoints;

    hEntity     workplane; // or Entity::FREE_IN_3D
    hGroup      group;
    hStyle      style;

    bool        construction;
    NameStr     str;
    NameStr     font;

    static hParam AddParam(ParamList *param, hParam hp);
    void Generate(EntityList *entity, ParamList *param);

    char *DescriptionString(void);
};

#define MAX_POINTS_IN_ENTITY (12)
class EntityBase {
public:
    int         tag;
    hEntity     h;

    static const hEntity    FREE_IN_3D;
    static const hEntity    NO_ENTITY;

    static const int POINT_IN_3D            =  2000;
    static const int POINT_IN_2D            =  2001;
    static const int POINT_N_TRANS          =  2010;
    static const int POINT_N_ROT_TRANS      =  2011;
    static const int POINT_N_COPY           =  2012;
    static const int POINT_N_ROT_AA         =  2013;

    static const int NORMAL_IN_3D           =  3000;
    static const int NORMAL_IN_2D           =  3001;
    static const int NORMAL_N_COPY          =  3010;
    static const int NORMAL_N_ROT           =  3011;
    static const int NORMAL_N_ROT_AA        =  3012;

    static const int DISTANCE               =  4000;
    static const int DISTANCE_N_COPY        =  4001;

    static const int FACE_NORMAL_PT         =  5000;
    static const int FACE_XPROD             =  5001;
    static const int FACE_N_ROT_TRANS       =  5002;
    static const int FACE_N_TRANS           =  5003;
    static const int FACE_N_ROT_AA          =  5004;


    static const int WORKPLANE              = 10000;
    static const int LINE_SEGMENT           = 11000;
    static const int CUBIC                  = 12000;
    static const int CUBIC_PERIODIC         = 12001;
    static const int CIRCLE                 = 13000;
    static const int ARC_OF_CIRCLE          = 14000;
    static const int TTF_TEXT               = 15000;

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

    NameStr     str;
    NameStr     font;

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
};

class Entity : public EntityBase {
public:
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

    char *DescriptionString(void);
};

class EntReqTable {
public:
    typedef struct {
        int     reqType;
        int     entType;
        int     points;
        bool    useExtraPoints;
        bool    hasNormal;
        bool    hasDistance;
        char    *description;
    } TableEntry;

    static const TableEntry Table[];

    static char *DescriptionForRequest(int req);
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
};


class hConstraint {
public:
    uint32_t   v;

    inline hEquation equation(int i);
};

class ConstraintBase {
public:
    int         tag;
    hConstraint h;

    static const hConstraint NO_CONSTRAINT;

    static const int POINTS_COINCIDENT      =  20;
    static const int PT_PT_DISTANCE         =  30;
    static const int PT_PLANE_DISTANCE      =  31;
    static const int PT_LINE_DISTANCE       =  32;
    static const int PT_FACE_DISTANCE       =  33;
    static const int PROJ_PT_DISTANCE       =  34;
    static const int PT_IN_PLANE            =  41;
    static const int PT_ON_LINE             =  42;
    static const int PT_ON_FACE             =  43;
    static const int EQUAL_LENGTH_LINES     =  50;
    static const int LENGTH_RATIO           =  51;
    static const int EQ_LEN_PT_LINE_D       =  52;
    static const int EQ_PT_LN_DISTANCES     =  53;
    static const int EQUAL_ANGLE            =  54;
    static const int EQUAL_LINE_ARC_LEN     =  55;
    static const int SYMMETRIC              =  60;
    static const int SYMMETRIC_HORIZ        =  61;
    static const int SYMMETRIC_VERT         =  62;
    static const int SYMMETRIC_LINE         =  63;
    static const int AT_MIDPOINT            =  70;
    static const int HORIZONTAL             =  80;
    static const int VERTICAL               =  81;
    static const int DIAMETER               =  90;
    static const int PT_ON_CIRCLE           = 100;
    static const int SAME_ORIENTATION       = 110;
    static const int ANGLE                  = 120;
    static const int PARALLEL               = 121;
    static const int PERPENDICULAR          = 122;
    static const int ARC_LINE_TANGENT       = 123;
    static const int CUBIC_LINE_TANGENT     = 124;
    static const int CURVE_CURVE_TANGENT    = 125;
    static const int EQUAL_RADIUS           = 130;
    static const int WHERE_DRAGGED          = 200;

    static const int COMMENT                = 1000;

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
    NameStr     comment;    // since comments are represented as constraints

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
};

class Constraint : public ConstraintBase {
public:
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
    char *Label(void);
    void DoArcForAngle(Vector a0, Vector da, Vector b0, Vector db,
                        Vector offset, Vector *ref);
    void DoLineWithArrows(Vector ref, Vector a, Vector b, bool onlyOneExt);
    int DoLineTrimmedAgainstBox(Vector ref, Vector a, Vector b);
    void DoLabel(Vector ref, Vector *labelPos, Vector gr, Vector gu);
    void StippledLine(Vector a, Vector b);
    void DoProjectedPoint(Vector *p);
    void DoEqualLenTicks(Vector a, Vector b, Vector gn);
    void DoEqualRadiusTicks(hEntity he);

    char *DescriptionString(void);

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
};


class Style {
public:
    int         tag;
    hStyle      h;

    // If an entity has no style, then it will be colored according to
    // whether the group that it's in is active or not, whether it's
    // construction or not, and so on.
    static const int NO_STYLE       = 0;

    static const int ACTIVE_GRP     = 1;
    static const int CONSTRUCTION   = 2;
    static const int INACTIVE_GRP   = 3;
    static const int DATUM          = 4;
    static const int SOLID_EDGE     = 5;
    static const int CONSTRAINT     = 6;
    static const int SELECTED       = 7;
    static const int HOVERED        = 8;
    static const int CONTOUR_FILL   = 9;
    static const int NORMALS        = 10;
    static const int ANALYZE        = 11;
    static const int DRAW_ERROR     = 12;
    static const int DIM_SOLID      = 13;

    static const int FIRST_CUSTOM   = 0x100;

    NameStr     name;

    static const int UNITS_AS_PIXELS   = 0;
    static const int UNITS_AS_MM       = 1;
    double      width;
    int         widthAs;
    double      textHeight;
    int         textHeightAs;
    static const int ORIGIN_LEFT       = 0x01;
    static const int ORIGIN_RIGHT      = 0x02;
    static const int ORIGIN_BOT        = 0x04;
    static const int ORIGIN_TOP        = 0x08;
    int         textOrigin;
    double      textAngle;
    uint32_t       color;
    bool        filled;
    uint32_t       fillColor;
    bool        visible;
    bool        exportable;

    // The default styles, for entities that don't have a style assigned yet,
    // and for datums and such.
    typedef struct {
        hStyle  h;
        char    *cnfPrefix;
        uint32_t   color;
        double  width;
    } Default;
    static const Default Defaults[];

    static char *CnfColor(char *prefix);
    static char *CnfWidth(char *prefix);
    static char *CnfPrefixToName(char *prefix);

    static void CreateAllDefaultStyles(void);
    static void CreateDefaultStyle(hStyle h);
    static void FreezeDefaultStyles(void);
    static void LoadFactoryDefaults(void);

    static void AssignSelectionToStyle(uint32_t v);
    static uint32_t CreateCustomStyle(void);

    static uint32_t RewriteColor(uint32_t rgb);

    static Style *Get(hStyle hs);
    static uint32_t Color(hStyle hs, bool forExport=false);
    static uint32_t FillColor(hStyle hs, bool forExport=false);
    static float Width(hStyle hs);
    static uint32_t Color(int hs, bool forExport=false);
    static float Width(int hs);
    static double WidthMm(int hs);
    static double TextHeight(hStyle hs);
    static bool Exportable(int hs);
    static hStyle ForEntity(hEntity he);

    char *DescriptionString(void);
};


inline hEntity hGroup::entity(int i)
    { hEntity r; r.v = 0x80000000 | (v << 16) | i; return r; }
inline hParam hGroup::param(int i)
    { hParam r; r.v = 0x80000000 | (v << 16) | i; return r; }
inline hEquation hGroup::equation(int i)
    { hEquation r; r.v = (v << 16) | 0x80000000 | i; return r; }

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
inline hGroup hEntity::group(void)
    { hGroup r; r.v = (v >> 16) & 0x3fff; return r; }
inline hEquation hEntity::equation(int i)
    { if(i != 0) oops(); hEquation r; r.v = v | 0x40000000; return r; }

inline hRequest hParam::request(void)
    { hRequest r; r.v = (v >> 16); return r; }


inline hEquation hConstraint::equation(int i)
    { hEquation r; r.v = (v << 16) | i; return r; }

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
    NameStr     str;
    NameStr     font;
    bool        construction;

    Vector      point[MAX_POINTS_IN_ENTITY];
    double      distance;

    hEntity     oldEnt;
    hEntity     oldPointEnt[MAX_POINTS_IN_ENTITY];
    hRequest    newReq;
};

#endif
