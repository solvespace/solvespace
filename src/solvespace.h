//-----------------------------------------------------------------------------
// All declarations not grouped specially elsewhere.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_H
#define SOLVESPACE_H

#include "resource.h"
#include "platform/platform.h"
#include "platform/gui.h"

#include <cctype>
#include <climits>
#include <cmath>
#include <csetjmp>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <functional>
#include <locale>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define EIGEN_NO_DEBUG
#undef Success
#include <Eigen/SparseCore>


#ifdef LIBRARY
#   define ENTITY EntityBase
#   define CONSTRAINT ConstraintBase
#   define GROUP GroupBase
#else
#   define ENTITY Entity
#   define CONSTRAINT Constraint
#   define GROUP Group
#endif

// We declare these in advance instead of simply using FT_Library
// (defined as typedef FT_LibraryRec_* FT_Library) because including
// freetype.h invokes indescribable horrors and we would like to avoid
// doing that every time we include solvespace.h.
struct FT_LibraryRec_;
struct FT_FaceRec_;

typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;

// The few floating-point equality comparisons in SolveSpace have been
// carefully considered, so we disable the -Wfloat-equal warning for them
#ifdef __clang__
#   define EXACT(expr) \
        (_Pragma("clang diagnostic push") \
         _Pragma("clang diagnostic ignored \"-Wfloat-equal\"") \
         (expr) \
         _Pragma("clang diagnostic pop"))
#else
#   define EXACT(expr) (expr)
#endif

// Debugging functions
#if defined(__GNUC__)
#define ssassert(condition, message) \
    do { \
        if(__builtin_expect((condition), true) == false) { \
            SolveSpace::AssertFailure(__FILE__, __LINE__, __func__, #condition, message); \
            __builtin_unreachable(); \
        } \
    } while(0)
#else
#define ssassert(condition, message) \
    do { \
        if((condition) == false) { \
            SolveSpace::AssertFailure(__FILE__, __LINE__, __func__, #condition, message); \
            abort(); \
        } \
    } while(0)
#endif

#define dbp SolveSpace::Platform::DebugPrint
#define DBPTRI(tri) \
    dbp("tri: (%.3f %.3f %.3f) (%.3f %.3f %.3f) (%.3f %.3f %.3f)", \
        CO((tri).a), CO((tri).b), CO((tri).c))

namespace SolveSpace {

using std::min;
using std::max;
using std::swap;
using std::fabs;

[[noreturn]]
void AssertFailure(const char *file, unsigned line, const char *function,
                   const char *condition, const char *message);

#if defined(__GNUC__)
__attribute__((__format__ (__printf__, 1, 2)))
#endif
std::string ssprintf(const char *fmt, ...);

inline bool IsReasonable(double x) {
    return std::isnan(x) || x > 1e11 || x < -1e11;
}

inline int WRAP(int v, int n) {
    // Clamp it to the range [0, n)
    while(v >= n) v -= n;
    while(v < 0) v += n;
    return v;
}
inline double WRAP_NOT_0(double v, double n) {
    // Clamp it to the range (0, n]
    while(v > n) v -= n;
    while(v <= 0) v += n;
    return v;
}
inline double WRAP_SYMMETRIC(double v, double n) {
    // Clamp it to the range (-n/2, n/2]
    while(v >   n/2) v -= n;
    while(v <= -n/2) v += n;
    return v;
}

#define CO(v) (v).x, (v).y, (v).z

static constexpr double ANGLE_COS_EPS =  1e-6;
static constexpr double LENGTH_EPS    =  1e-6;
static constexpr double VERY_POSITIVE =  1e10;
static constexpr double VERY_NEGATIVE = -1e10;


using Platform::AllocTemporary;
using Platform::FreeAllTemporary;

class Expr;
class ExprVector;
class ExprQuaternion;
class RgbaColor;
enum class Command : uint32_t;

enum class Unit : uint32_t {
    MM = 0,
    INCHES,
    METERS,
    FEET_INCHES
};

template<class Key, class T>
using handle_map = std::map<Key, T>;

class GROUP;
class Group;
class SSurface;
#include "dsc.h"
#include "polygon.h"
#include "srf/surface.h"
#include "render/render.h"

class Entity;
class hEntity;
class Param;
class hParam;
typedef IdList<Entity,hEntity> EntityList;
typedef IdList<Param,hParam> ParamList;

enum class SolveResult : uint32_t {
    OKAY                     = 0,
    DIDNT_CONVERGE           = 10,
    REDUNDANT_OKAY           = 11,
    REDUNDANT_DIDNT_CONVERGE = 12,
    TOO_MANY_UNKNOWNS        = 20
};


#include "sketch.h"
#include "ui.h"
#include "expr.h"


// Utility functions that are provided in the platform-independent code.
class utf8_iterator : std::iterator<std::forward_iterator_tag, char32_t> {
    const char *p, *n;
public:
    utf8_iterator(const char *p) : p(p), n(NULL) {}
    bool           operator==(const utf8_iterator &i) const { return p==i.p; }
    bool           operator!=(const utf8_iterator &i) const { return p!=i.p; }
    ptrdiff_t      operator- (const utf8_iterator &i) const { return p -i.p; }
    utf8_iterator& operator++()    { **this; p=n; n=NULL; return *this; }
    utf8_iterator  operator++(int) { utf8_iterator t(*this); operator++(); return t; }
    char32_t       operator*();
    const char*    ptr() const { return p; }
};
class ReadUTF8 {
    const std::string &str;
public:
    ReadUTF8(const std::string &str) : str(str) {}
    utf8_iterator begin() const { return utf8_iterator(&str[0]); }
    utf8_iterator end()   const { return utf8_iterator(&str[0] + str.length()); }
};


#define arraylen(x) (sizeof((x))/sizeof((x)[0]))
#define PI (3.1415926535897931)
void MakeMatrix(double *mat, double a11, double a12, double a13, double a14,
                             double a21, double a22, double a23, double a24,
                             double a31, double a32, double a33, double a34,
                             double a41, double a42, double a43, double a44);
void MultMatrix(double *mata, double *matb, double *matr);

int64_t GetMilliseconds();
void Message(const char *fmt, ...);
void MessageAndRun(std::function<void()> onDismiss, const char *fmt, ...);
void Error(const char *fmt, ...);

class System {
public:
    enum { MAX_UNKNOWNS = 2048 };

    EntityList                      entity;
    ParamList                       param;
    IdList<Equation,hEquation>      eq;

    // A list of parameters that are being dragged; these are the ones that
    // we should put as close as possible to their initial positions.
    List<hParam>                    dragged;

    enum {
        // In general, the tag indicates the subsys that a variable/equation
        // has been assigned to; these are exceptions for variables:
        VAR_SUBSTITUTED      = 10000,
        VAR_DOF_TEST         = 10001,
        // and for equations:
        EQ_SUBSTITUTED       = 20000
    };

    // The system Jacobian matrix
    struct {
        // The corresponding equation for each row
        std::vector<Equation *> eq;

        // The corresponding parameter for each column
        std::vector<hParam>     param;

        // We're solving AX = B
        int m, n;
        struct {
            // This only observes the Expr - does not own them!
            Eigen::SparseMatrix<Expr *> sym;
            Eigen::SparseMatrix<double> num;
        } A;

        Eigen::VectorXd scale;
        Eigen::VectorXd X;

        struct {
            // This only observes the Expr - does not own them!
            std::vector<Expr *> sym;
            Eigen::VectorXd     num;
        } B;
    } mat;

    static const double CONVERGE_TOLERANCE;
    int CalculateRank();
    bool TestRank(int *dof = NULL);
    static bool SolveLinearSystem(const Eigen::SparseMatrix<double> &A,
                                  const Eigen::VectorXd &B, Eigen::VectorXd *X);
    bool SolveLeastSquares();

    bool WriteJacobian(int tag);
    void EvalJacobian();

    void WriteEquationsExceptFor(hConstraint hc, GROUP *g);
    void FindWhichToRemoveToFixJacobian(GROUP *g, List<hConstraint> *bad,
                                        bool forceDofCheck);
    void SolveBySubstitution();

    bool IsDragged(hParam p);

    bool NewtonSolve(int tag);

    void MarkParamsFree(bool findFree);

    SolveResult Solve(GROUP *g, int *rank = NULL, int *dof = NULL,
                      List<hConstraint> *bad = NULL,
                      bool andFindBad = false, bool andFindFree = false,
                      bool forceDofCheck = false);

    SolveResult SolveRank(GROUP *g, int *rank = NULL, int *dof = NULL,
                          List<hConstraint> *bad = NULL,
                          bool andFindBad = false, bool andFindFree = false);

    void Clear();
    Param *GetLastParamSubstitution(Param *p);
    void SubstituteParamsByLast(Expr *e);
    void SortSubstitutionByDragged(Param *p);
};

#include "ttf.h"

class StepFileWriter {
public:
    void ExportSurfacesTo(const Platform::Path &filename);
    void WriteHeader();
    void WriteProductHeader();
    int ExportCurve(SBezier *sb);
    int ExportCurveLoop(SBezierLoop *loop, bool inner);
    void ExportSurface(SSurface *ss, SBezierList *sbl);
    void WriteWireframe();
    void WriteFooter();

    List<int> curves;
    List<int> advancedFaces;
    FILE *f;
    int id;
};

class VectorFileWriter {
protected:
    Vector u, v, n, origin;
    double cameraTan, scale;

public:
    FILE *f;
    Platform::Path filename;
    Vector ptMin, ptMax;

    static double MmToPts(double mm);

    static VectorFileWriter *ForFile(const Platform::Path &filename);

    void SetModelviewProjection(const Vector &u, const Vector &v, const Vector &n,
                                const Vector &origin, double cameraTan, double scale);
    Vector Transform(Vector &pos) const;

    void OutputLinesAndMesh(SBezierLoopSetSet *sblss, SMesh *sm);

    void BezierAsPwl(SBezier *sb);
    void BezierAsNonrationalCubic(SBezier *sb, int depth=0);

    virtual void StartPath(RgbaColor strokeRgb, double lineWidth,
                            bool filled, RgbaColor fillRgb, hStyle hs) = 0;
    virtual void FinishPath(RgbaColor strokeRgb, double lineWidth,
                            bool filled, RgbaColor fillRgb, hStyle hs) = 0;
    virtual void Bezier(SBezier *sb) = 0;
    virtual void Triangle(STriangle *tr) = 0;
    virtual bool OutputConstraints(IdList<Constraint,hConstraint> *) { return false; }
    virtual void Background(RgbaColor color) = 0;
    virtual void StartFile() = 0;
    virtual void FinishAndCloseFile() = 0;
    virtual bool HasCanvasSize() const = 0;
    virtual bool CanOutputMesh() const = 0;
};
class DxfFileWriter : public VectorFileWriter {
public:
    struct BezierPath {
        std::vector<SBezier *> beziers;
    };

    std::vector<BezierPath>         paths;
    IdList<Constraint,hConstraint> *constraint;

    static const char *lineTypeName(StipplePattern stippleType);

    bool OutputConstraints(IdList<Constraint,hConstraint> *constraint) override;

    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void Triangle(STriangle *tr) override;
    void Bezier(SBezier *sb) override;
    void Background(RgbaColor color) override;
    void StartFile() override;
    void FinishAndCloseFile() override;
    bool HasCanvasSize() const override { return false; }
    bool CanOutputMesh() const override { return false; }
    bool NeedToOutput(Constraint *c);
};
class EpsFileWriter : public VectorFileWriter {
public:
    Vector prevPt;
    void MaybeMoveTo(Vector s, Vector f);

    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void Triangle(STriangle *tr) override;
    void Bezier(SBezier *sb) override;
    void Background(RgbaColor color) override;
    void StartFile() override;
    void FinishAndCloseFile() override;
    bool HasCanvasSize() const override { return true; }
    bool CanOutputMesh() const override { return true; }
};
class PdfFileWriter : public VectorFileWriter {
public:
    uint32_t xref[10];
    uint32_t bodyStart;
    Vector prevPt;
    void MaybeMoveTo(Vector s, Vector f);

    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void Triangle(STriangle *tr) override;
    void Bezier(SBezier *sb) override;
    void Background(RgbaColor color) override;
    void StartFile() override;
    void FinishAndCloseFile() override;
    bool HasCanvasSize() const override { return true; }
    bool CanOutputMesh() const override { return true; }
};
class SvgFileWriter : public VectorFileWriter {
public:
    Vector prevPt;
    void MaybeMoveTo(Vector s, Vector f);

    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void Triangle(STriangle *tr) override;
    void Bezier(SBezier *sb) override;
    void Background(RgbaColor color) override;
    void StartFile() override;
    void FinishAndCloseFile() override;
    bool HasCanvasSize() const override { return true; }
    bool CanOutputMesh() const override { return true; }
};
class HpglFileWriter : public VectorFileWriter {
public:
    static double MmToHpglUnits(double mm);
    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void Triangle(STriangle *tr) override;
    void Bezier(SBezier *sb) override;
    void Background(RgbaColor color) override;
    void StartFile() override;
    void FinishAndCloseFile() override;
    bool HasCanvasSize() const override { return false; }
    bool CanOutputMesh() const override { return false; }
};
class Step2dFileWriter : public VectorFileWriter {
    StepFileWriter sfw;
    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void Triangle(STriangle *tr) override;
    void Bezier(SBezier *sb) override;
    void Background(RgbaColor color) override;
    void StartFile() override;
    void FinishAndCloseFile() override;
    bool HasCanvasSize() const override { return false; }
    bool CanOutputMesh() const override { return false; }
};
class GCodeFileWriter : public VectorFileWriter {
public:
    SEdgeList sel;
    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void Triangle(STriangle *tr) override;
    void Bezier(SBezier *sb) override;
    void Background(RgbaColor color) override;
    void StartFile() override;
    void FinishAndCloseFile() override;
    bool HasCanvasSize() const override { return false; }
    bool CanOutputMesh() const override { return false; }
};

class Sketch {
public:
    // These are user-editable, and define the sketch.
    IdList<GROUP,hGroup>            group;
    List<hGroup>                    groupOrder;
    IdList<CONSTRAINT,hConstraint>  constraint;
    IdList<Request,hRequest>        request;
    IdList<Style,hStyle>            style;

    // These are generated from the above.
    IdList<ENTITY,hEntity>          entity;
    IdList<Param,hParam>            param;

    inline CONSTRAINT *GetConstraint(hConstraint h)
        { return constraint.FindById(h); }
    inline ENTITY  *GetEntity (hEntity  h) { return entity. FindById(h); }
    inline Param   *GetParam  (hParam   h) { return param.  FindById(h); }
    inline Request *GetRequest(hRequest h) { return request.FindById(h); }
    inline GROUP   *GetGroup  (hGroup   h) { return group.  FindById(h); }
    // Styles are handled a bit differently.

    GROUP *activeGroup;

    inline std::vector<Param> GetParams() {
        std::vector<Param> params;
        for(Param &p : param) {
            params.push_back(p);
        }
        return params;
    }

    inline std::vector<double> GetParams(std::vector<hParam> phs) {
        std::vector<double> params;
        for(hParam &ph : phs) {
            Param *par = param.FindById(ph);
            params.push_back(par->val);
        }
        return params;
    }

    inline std::vector<ENTITY> GetEntities() {
        std::vector<ENTITY> entities;
        for(ENTITY &e : entity) {
            entities.push_back(e);
        }
        return entities;
    }

    inline std::vector<CONSTRAINT> GetConstraints() {
        std::vector<CONSTRAINT> constraints;
        for(CONSTRAINT &c : constraint) {
            constraints.push_back(c);
        }
        return constraints;
    }

    // group

    inline void AddGroup(GROUP *g) {
        group.AddAndAssignId(g);
    }

    inline void SetActiveGroup(GROUP *g) {
        activeGroup = g;
    }

    inline GROUP* GetActiveGroup() {
        return activeGroup;
    }

    // param

    inline Param AddParam(double val) {
        Param pa = {};
        pa.val   = val;
        param.AddAndAssignId(&pa);
        return pa;
    }

    // entities

    inline ENTITY AddPoint2D(double u, double v, ENTITY workplane) {
        if(!activeGroup) {
            throw std::invalid_argument("missing active group! aborting...");
        }
        Param up      = AddParam(u);
        Param vp      = AddParam(v);
        ENTITY e  = {};
        e.type        = ENTITY::Type::POINT_IN_2D;
        e.group.v     = activeGroup->h.v;
        e.workplane.v = workplane.h.v;
        e.param[0].v  = up.h.v;
        e.param[1].v  = vp.h.v;
        entity.AddAndAssignId(&e);
        return e;
    }

    inline ENTITY AddPoint3D(double x, double y, double z) {
        if(!activeGroup) {
            throw std::invalid_argument("missing active group! aborting...");
        }
        Param xp      = AddParam(x);
        Param yp      = AddParam(y);
        Param zp      = AddParam(z);
        ENTITY e  = {};
        e.type        = ENTITY::Type::POINT_IN_3D;
        e.group.v     = activeGroup->h.v;
        e.workplane.v = ENTITY::_FREE_IN_3D.h.v;
        e.param[0].v  = xp.h.v;
        e.param[1].v  = yp.h.v;
        e.param[2].v  = zp.h.v;
        entity.AddAndAssignId(&e);
        return e;
    }

    inline ENTITY AddNormal2D(ENTITY workplane) {
        if(!activeGroup) {
            throw std::invalid_argument("missing active group! aborting...");
        } else if(!workplane.IsWorkplane()) {
            throw std::invalid_argument("workplane argument is not a workplane");
        }
        ENTITY e  = {};
        e.type        = ENTITY::Type::NORMAL_IN_2D;
        e.group.v     = activeGroup->h.v;
        e.workplane.v = workplane.h.v;
        entity.AddAndAssignId(&e);
        return e;
    }

    inline ENTITY AddNormal3D(double qw, double qx, double qy, double qz) {
        if(!activeGroup) {
            throw std::invalid_argument("missing active group! aborting...");
        }
        Param wp      = AddParam(qw);
        Param xp      = AddParam(qx);
        Param yp      = AddParam(qy);
        Param zp      = AddParam(qz);
        ENTITY e  = {};
        e.type        = ENTITY::Type::NORMAL_IN_3D;
        e.group.v     = activeGroup->h.v;
        e.workplane.v = ENTITY::_FREE_IN_3D.h.v;
        e.param[0].v  = wp.h.v;
        e.param[1].v  = xp.h.v;
        e.param[2].v  = yp.h.v;
        e.param[3].v  = zp.h.v;
        entity.AddAndAssignId(&e);
        return e;
    }

    inline ENTITY AddDistance(double value, ENTITY workplane) {
        if(!activeGroup) {
            throw std::invalid_argument("missing active group! aborting...");
        } else if(!workplane.IsWorkplane()) {
            throw std::invalid_argument("workplane argument is not a workplane");
        }
        Param valuep  = AddParam(value);
        ENTITY e  = {};
        e.type        = ENTITY::Type::DISTANCE;
        e.group.v     = activeGroup->h.v;
        e.workplane.v = workplane.h.v;
        e.param[0].v  = valuep.h.v;
        entity.AddAndAssignId(&e);
        return e;
    }

    inline ENTITY AddLine2D(ENTITY ptA, ENTITY ptB, ENTITY workplane) {
        if(!activeGroup) {
            throw std::invalid_argument("missing active group! aborting...");
        } else if(!workplane.IsWorkplane()) {
            throw std::invalid_argument("workplane argument is not a workplane");
        } else if(!ptA.IsPoint2D()) {
            throw std::invalid_argument("ptA argument is not a 2d point");
        } else if(!ptB.IsPoint2D()) {
            throw std::invalid_argument("ptB argument is not a 2d point");
        }
        ENTITY e  = {};
        e.type        = ENTITY::Type::LINE_SEGMENT;
        e.group.v     = activeGroup->h.v;
        e.workplane.v = workplane.h.v;
        e.point[0].v  = ptA.h.v;
        e.point[1].v  = ptB.h.v;
        entity.AddAndAssignId(&e);
        return e;
    }

    inline ENTITY AddLine3D(ENTITY ptA, ENTITY ptB) {
        if(!activeGroup) {
            throw std::invalid_argument("missing active group! aborting...");
        } else if(!ptA.IsPoint3D()) {
            throw std::invalid_argument("ptA argument is not a 3d point");
        } else if(!ptB.IsPoint3D()) {
            throw std::invalid_argument("ptB argument is not a 3d point");
        }
        ENTITY e  = {};
        e.type        = ENTITY::Type::LINE_SEGMENT;
        e.group.v     = activeGroup->h.v;
        e.workplane.v = ENTITY::_FREE_IN_3D.h.v;
        e.point[0].v  = ptA.h.v;
        e.point[1].v  = ptB.h.v;
        entity.AddAndAssignId(&e);
        return e;
    }

    inline ENTITY AddCubic(ENTITY ptA, ENTITY ptB, ENTITY ptC, ENTITY ptD,
                               ENTITY workplane) {
        if(!activeGroup) {
            throw std::invalid_argument("missing active group! aborting...");
        } else if(!workplane.IsWorkplane()) {
            throw std::invalid_argument("workplane argument is not a workplane");
        } else if(!ptA.IsPoint2D()) {
            throw std::invalid_argument("ptA argument is not a 2d point");
        } else if(!ptB.IsPoint2D()) {
            throw std::invalid_argument("ptB argument is not a 2d point");
        } else if(!ptC.IsPoint2D()) {
            throw std::invalid_argument("ptC argument is not a 2d point");
        } else if(!ptD.IsPoint2D()) {
            throw std::invalid_argument("ptD argument is not a 2d point");
        }
        ENTITY e  = {};
        e.type        = ENTITY::Type::CUBIC;
        e.group.v     = activeGroup->h.v;
        e.workplane.v = workplane.h.v;
        e.point[0].v  = ptA.h.v;
        e.point[1].v  = ptB.h.v;
        e.point[2].v  = ptC.h.v;
        e.point[3].v  = ptD.h.v;
        entity.AddAndAssignId(&e);
        return e;
    }

    inline ENTITY AddArc(ENTITY normal, ENTITY center, ENTITY start, ENTITY end,
                             ENTITY workplane) {
        if(!activeGroup) {
            throw std::invalid_argument("missing active group! aborting...");
        } else if(!workplane.IsWorkplane()) {
            throw std::invalid_argument("workplane argument is not a workplane");
        } else if(!normal.IsNormal3D()) {
            throw std::invalid_argument("normal argument is not a 3d normal");
        } else if(!center.IsPoint2D()) {
            throw std::invalid_argument("center argument is not a 2d point");
        } else if(!start.IsPoint2D()) {
            throw std::invalid_argument("start argument is not a 2d point");
        } else if(!end.IsPoint2D()) {
            throw std::invalid_argument("end argument is not a 2d point");
        }
        ENTITY e  = {};
        e.type        = ENTITY::Type::ARC_OF_CIRCLE;
        e.group.v     = activeGroup->h.v;
        e.workplane.v = workplane.h.v;
        e.normal.v    = normal.h.v;
        e.point[0].v  = center.h.v;
        e.point[1].v  = start.h.v;
        e.point[2].v  = end.h.v;
        entity.AddAndAssignId(&e);
        return e;
    }

    inline ENTITY AddCircle(ENTITY normal, ENTITY center, ENTITY radius,
                                ENTITY workplane) {
        if(!activeGroup) {
            throw std::invalid_argument("missing active group! aborting...");
        } else if(!workplane.IsWorkplane()) {
            throw std::invalid_argument("workplane argument is not a workplane");
        } else if(!normal.IsNormal3D()) {
            throw std::invalid_argument("normal argument is not a 3d normal");
        } else if(!center.IsPoint2D()) {
            throw std::invalid_argument("center argument is not a 2d point");
        } else if(!radius.IsDistance()) {
            throw std::invalid_argument("radius argument is not a distance");
        }
        ENTITY e  = {};
        e.type        = ENTITY::Type::CIRCLE;
        e.group.v     = activeGroup->h.v;
        e.workplane.v = workplane.h.v;
        e.normal.v    = normal.h.v;
        e.point[0].v  = center.h.v;
        e.distance.v  = radius.h.v;
        entity.AddAndAssignId(&e);
        return e;
    }

    inline ENTITY AddWorkplane(ENTITY origin, ENTITY nm) {
        if(!activeGroup) {
            throw std::invalid_argument("missing active group! aborting...");
        }
        ENTITY e  = {};
        e.type        = ENTITY::Type::WORKPLANE;
        e.group.v     = activeGroup->h.v;
        e.workplane.v = ENTITY::_FREE_IN_3D.h.v;
        e.point[0].v  = origin.h.v;
        e.normal.v    = nm.h.v;
        entity.AddAndAssignId(&e);
        return e;
    }

    inline ENTITY Create2DBase() {
        if(!activeGroup) {
            throw std::invalid_argument("missing active group! aborting...");
        }
        Vector u      = Vector::From(1, 0, 0);
        Vector v      = Vector::From(0, 1, 0);
        Quaternion q  = Quaternion::From(u, v);
        ENTITY nm = AddNormal3D(q.w, q.vx, q.vy, q.vz);
        return AddWorkplane(AddPoint3D(0, 0, 0), nm);
    }

    // constraints

    inline CONSTRAINT AddConstraint(
        typename CONSTRAINT::Type type, ENTITY workplane, double val, ENTITY ptA,
        ENTITY ptB = ENTITY::_NO_ENTITY, ENTITY entityA = ENTITY::_NO_ENTITY,
        ENTITY entityB = ENTITY::_NO_ENTITY, ENTITY entityC = ENTITY::_NO_ENTITY,
        ENTITY entityD = ENTITY::_NO_ENTITY, int other = 0, int other2 = 0) {
        if(!activeGroup) {
            throw std::invalid_argument("missing active group! aborting...");
        }
        CONSTRAINT c = {};
        c.type           = type;
        c.group.v        = activeGroup->h.v;
        c.workplane.v    = workplane.h.v;
        c.valA           = val;
        c.ptA.v          = ptA.h.v;
        c.ptB.v          = ptB.h.v;
        c.entityA.v      = entityA.h.v;
        c.entityB.v      = entityB.h.v;
        c.entityC.v      = entityC.h.v;
        c.entityD.v      = entityD.h.v;
        c.other          = other ? true : false;
        c.other2         = other2 ? true : false;
        constraint.AddAndAssignId(&c);
        return c;
    }

    inline CONSTRAINT Coincident(ENTITY entityA, ENTITY entityB,
                                     ENTITY workplane = ENTITY::_FREE_IN_3D) {
        if(entityA.IsPoint() && entityB.IsPoint()) {
            return AddConstraint(CONSTRAINT::Type::POINTS_COINCIDENT, workplane, 0., entityA,
                                 entityB);
        } else if(entityA.IsPoint() && entityB.IsWorkplane()) {
            return AddConstraint(CONSTRAINT::Type::PT_IN_PLANE, ENTITY::_FREE_IN_3D, 0., entityA, ENTITY::_NO_ENTITY, entityB);
        } else if(entityA.IsPoint() && entityB.IsLine()) {
            return AddConstraint(CONSTRAINT::Type::PT_ON_LINE, workplane, 0., entityA, ENTITY::_NO_ENTITY, entityB);
        } else if(entityA.IsPoint() && (entityB.IsCircle() || entityB.IsArc())) {
            return AddConstraint(CONSTRAINT::Type::PT_ON_CIRCLE, workplane, 0., entityA, ENTITY::_NO_ENTITY, entityB);
        }
        throw std::invalid_argument("Invalid arguments for coincident constraint");
    }

    inline CONSTRAINT Distance(ENTITY entityA, ENTITY entityB, double value,
                                   ENTITY workplane) {
        if(entityA.IsPoint() && entityB.IsPoint()) {
            return AddConstraint(CONSTRAINT::Type::PT_PT_DISTANCE, workplane, value, entityA,
                                 entityB);
        } else if(entityA.IsPoint() && entityB.IsWorkplane() && workplane.Is3D()) {
            return AddConstraint(CONSTRAINT::Type::PT_PLANE_DISTANCE, entityB, value, entityA,
                                 ENTITY::_NO_ENTITY, entityB);
        } else if(entityA.IsPoint() && entityB.IsLine()) {
            return AddConstraint(CONSTRAINT::Type::PT_LINE_DISTANCE, workplane, value, entityA,
                                 ENTITY::_NO_ENTITY, entityB);
        }
        throw std::invalid_argument("Invalid arguments for distance constraint");
    }

    inline CONSTRAINT Equal(ENTITY entityA, ENTITY entityB,
                                ENTITY workplane = ENTITY::_FREE_IN_3D) {
        if(entityA.IsLine() && entityB.IsLine()) {
            return AddConstraint(CONSTRAINT::Type::EQUAL_LENGTH_LINES, workplane, 0.,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, entityA, entityB);
        } else if(entityA.IsLine() && (entityB.IsArc() || entityB.IsCircle())) {
            return AddConstraint(CONSTRAINT::Type::EQUAL_LINE_ARC_LEN, workplane, 0.,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, entityA, entityB);
        } else if((entityA.IsArc() || entityA.IsCircle()) &&
                  (entityB.IsArc() || entityB.IsCircle())) {
            return AddConstraint(CONSTRAINT::Type::EQUAL_RADIUS, workplane, 0.,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, entityA, entityB);
        }
        throw std::invalid_argument("Invalid arguments for equal constraint");
    }

    inline CONSTRAINT EqualAngle(ENTITY entityA, ENTITY entityB, ENTITY entityC,
                                     ENTITY entityD,
                                     ENTITY workplane = ENTITY::_FREE_IN_3D) {
        if(entityA.IsLine2D() && entityB.IsLine2D() && entityC.IsLine2D() && entityD.IsLine2D()) {
            return AddConstraint(CONSTRAINT::Type::EQUAL_ANGLE, workplane, 0.,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, entityA, entityB,
                                 entityC, entityD);
        }
        throw std::invalid_argument("Invalid arguments for equal angle constraint");
    }

    inline CONSTRAINT EqualPointToLine(ENTITY entityA, ENTITY entityB,
                                           ENTITY entityC, ENTITY entityD,
                                           ENTITY workplane = ENTITY::_FREE_IN_3D) {
        if(entityA.IsPoint2D() && entityB.IsLine2D() && entityC.IsPoint2D() && entityD.IsLine2D()) {
            return AddConstraint(CONSTRAINT::Type::EQ_PT_LN_DISTANCES, workplane, 0., entityA,
                                 entityB, entityC, entityD);
        }
        throw std::invalid_argument("Invalid arguments for equal point to line constraint");
    }

    inline CONSTRAINT Ratio(ENTITY entityA, ENTITY entityB, double value,
                                ENTITY workplane = ENTITY::_FREE_IN_3D) {
        if(entityA.IsLine2D() && entityB.IsLine2D()) {
            return AddConstraint(CONSTRAINT::Type::LENGTH_RATIO, workplane, value,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, entityA, entityB);
        }
        throw std::invalid_argument("Invalid arguments for ratio constraint");
    }

    inline CONSTRAINT Symmetric(ENTITY entityA, ENTITY entityB,
                                    ENTITY entityC   = ENTITY::_NO_ENTITY,
                                    ENTITY workplane = ENTITY::_FREE_IN_3D) {
        if(entityA.IsPoint3D() && entityB.IsPoint3D() && entityC.IsWorkplane() &&
           workplane.IsFreeIn3D()) {
            return AddConstraint(CONSTRAINT::Type::SYMMETRIC, workplane, 0., entityA, entityB,
                                 entityC);
        } else if(entityA.IsPoint2D() && entityB.IsPoint2D() && entityC.IsWorkplane() &&
                  workplane.IsFreeIn3D()) {
            return AddConstraint(CONSTRAINT::Type::SYMMETRIC, entityC, 0., entityA, entityB,
                                 entityC);
        } else if(entityA.IsPoint2D() && entityB.IsPoint2D() && entityC.IsLine()) {
            if(workplane.IsFreeIn3D()) {
                throw std::invalid_argument("3d workplane given for a 2d constraint");
            }
            return AddConstraint(CONSTRAINT::Type::SYMMETRIC_LINE, workplane, 0., entityA,
                                 entityB, entityC);
        }
        throw std::invalid_argument("Invalid arguments for symmetric constraint");
    }

    inline CONSTRAINT SymmetricH(ENTITY ptA, ENTITY ptB,
                                     ENTITY workplane = ENTITY::_FREE_IN_3D) {
        if(workplane.IsFreeIn3D()) {
            throw std::invalid_argument("3d workplane given for a 2d constraint");
        } else if(ptA.IsPoint2D() && ptB.IsPoint2D()) {
            return AddConstraint(CONSTRAINT::Type::SYMMETRIC_HORIZ, workplane, 0., ptA, ptB);
        }
        throw std::invalid_argument("Invalid arguments for symmetric horizontal constraint");
    }

    inline CONSTRAINT SymmetricV(ENTITY ptA, ENTITY ptB,
                                     ENTITY workplane = ENTITY::_FREE_IN_3D) {
        if(workplane.IsFreeIn3D()) {
            throw std::invalid_argument("3d workplane given for a 2d constraint");
        } else if(ptA.IsPoint2D() && ptB.IsPoint2D()) {
            return AddConstraint(CONSTRAINT::Type::SYMMETRIC_VERT, workplane, 0., ptA, ptB);
        }
        throw std::invalid_argument("Invalid arguments for symmetric vertical constraint");
    }

    inline CONSTRAINT Midpoint(ENTITY ptA, ENTITY ptB,
                                   ENTITY workplane = ENTITY::_FREE_IN_3D) {
        if(ptA.IsPoint() && ptB.IsLine()) {
            return AddConstraint(CONSTRAINT::Type::AT_MIDPOINT, workplane, 0., ptA,
                                 ENTITY::_NO_ENTITY, ptB);
        }
        throw std::invalid_argument("Invalid arguments for midpoint constraint");
    }

    inline CONSTRAINT Horizontal(ENTITY entityA, ENTITY workplane,
                                     ENTITY entityB = ENTITY::_NO_ENTITY) {
        if(workplane.IsFreeIn3D()) {
            throw std::invalid_argument("Horizontal constraint is not supported in 3D");
        } else if(entityA.IsLine2D()) {
            return AddConstraint(CONSTRAINT::Type::HORIZONTAL, workplane, 0.,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, entityA);
        } else if(entityA.IsPoint2D() && entityB.IsPoint2D()) {
            return AddConstraint(CONSTRAINT::Type::HORIZONTAL, workplane, 0., entityA, entityB);
        }
        throw std::invalid_argument("Invalid arguments for horizontal constraint");
    }

    inline CONSTRAINT Vertical(ENTITY entityA, ENTITY workplane,
                                   ENTITY entityB = ENTITY::_NO_ENTITY) {
        if(workplane.IsFreeIn3D()) {
            throw std::invalid_argument("Vertical constraint is not supported in 3D");
        } else if(entityA.IsLine2D()) {
            return AddConstraint(CONSTRAINT::Type::VERTICAL, workplane, 0.,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, entityA);
        } else if(entityA.IsPoint2D() && entityB.IsPoint2D()) {
            return AddConstraint(CONSTRAINT::Type::VERTICAL, workplane, 0., entityA, entityB);
        }
        throw std::invalid_argument("Invalid arguments for horizontal constraint");
    }

    inline CONSTRAINT Diameter(ENTITY entityA, double value) {
        if(entityA.IsArc() || entityA.IsCircle()) {
            return AddConstraint(CONSTRAINT::Type::DIAMETER, ENTITY::_FREE_IN_3D, value,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, entityA);
        }
        throw std::invalid_argument("Invalid arguments for diameter constraint");
    }

    inline CONSTRAINT SameOrientation(ENTITY entityA, ENTITY entityB) {
        if(entityA.IsNormal3D() && entityB.IsNormal3D()) {
            return AddConstraint(CONSTRAINT::Type::SAME_ORIENTATION, ENTITY::_FREE_IN_3D,
                                 0., ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, entityA,
                                 entityB);
        }
        throw std::invalid_argument("Invalid arguments for same orientation constraint");
    }

    inline CONSTRAINT Angle(ENTITY entityA, ENTITY entityB, double value,
                                ENTITY workplane = ENTITY::_FREE_IN_3D,
                                bool inverse         = false) {
        if(entityA.IsLine2D() && entityB.IsLine2D()) {
            return AddConstraint(CONSTRAINT::Type::ANGLE, workplane, value,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, entityA, entityB,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, inverse);
        }
        throw std::invalid_argument("Invalid arguments for angle constraint");
    }

    inline CONSTRAINT Perpendicular(ENTITY entityA, ENTITY entityB,
                                        ENTITY workplane = ENTITY::_FREE_IN_3D,
                                        bool inverse         = false) {
        if(entityA.IsLine2D() && entityB.IsLine2D()) {
            return AddConstraint(CONSTRAINT::Type::PERPENDICULAR, workplane, 0.,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, entityA, entityB,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, inverse);
        }
        throw std::invalid_argument("Invalid arguments for perpendicular constraint");
    }

    inline CONSTRAINT Parallel(ENTITY entityA, ENTITY entityB,
                                   ENTITY workplane = ENTITY::_FREE_IN_3D) {
        if(entityA.IsLine2D() && entityB.IsLine2D()) {
            return AddConstraint(CONSTRAINT::Type::PARALLEL, workplane, 0.,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, entityA, entityB);
        }
        throw std::invalid_argument("Invalid arguments for parallel constraint");
    }

    inline CONSTRAINT Tangent(ENTITY entityA, ENTITY entityB,
                                  ENTITY workplane = ENTITY::_FREE_IN_3D) {
        if(entityA.IsArc() && entityB.IsLine2D()) {
            if(workplane.IsFreeIn3D()) {
                throw std::invalid_argument("3d workplane given for a 2d constraint");
            }
            Vector a1 = GetEntity(entityA.point[1])->PointGetNum(),
                   a2 = GetEntity(entityA.point[2])->PointGetNum();
            Vector l0 = GetEntity(entityB.point[0])->PointGetNum(),
                   l1 = GetEntity(entityB.point[1])->PointGetNum();
            bool other;
            if(l0.Equals(a1) || l1.Equals(a1)) {
                other = false;
            } else if(l0.Equals(a2) || l1.Equals(a2)) {
                other = true;
            } else {
                throw std::invalid_argument("The tangent arc and line segment must share an "
                                            "endpoint. Constrain them with Constrain -> "
                                            "On Point before constraining tangent.");
            }
            return AddConstraint(CONSTRAINT::Type::ARC_LINE_TANGENT, workplane, 0.,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, entityA, entityB,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, other);
        } else if(entityA.IsCubic() && entityB.IsLine2D() && workplane.IsFreeIn3D()) {
            Vector as = entityA.CubicGetStartNum(), af = entityA.CubicGetFinishNum();
            Vector l0 = GetEntity(entityB.point[0])->PointGetNum(),
                   l1 = GetEntity(entityB.point[1])->PointGetNum();
            bool other;
            if(l0.Equals(as) || l1.Equals(as)) {
                other = false;
            } else if(l0.Equals(af) || l1.Equals(af)) {
                other = true;
            } else {
                throw std::invalid_argument("The tangent cubic and line segment must share an "
                                            "endpoint. Constrain them with Constrain -> "
                                            "On Point before constraining tangent.");
            }
            return AddConstraint(CONSTRAINT::Type::CUBIC_LINE_TANGENT, workplane, 0.,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, entityA, entityB,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, other);
        } else if((entityA.IsArc() || entityA.IsCubic()) &&
                  (entityB.IsArc() || entityB.IsCubic())) {
            if(workplane.IsFreeIn3D()) {
                throw std::invalid_argument("3d workplane given for a 2d constraint");
            }
            Vector as = entityA.EndpointStart(), af = entityA.EndpointFinish(),
                   bs = entityB.EndpointStart(), bf = entityB.EndpointFinish();
            bool other;
            bool other2;
            if(as.Equals(bs)) {
                other  = false;
                other2 = false;
            } else if(as.Equals(bf)) {
                other  = false;
                other2 = true;
            } else if(af.Equals(bs)) {
                other  = true;
                other2 = false;
            } else if(af.Equals(bf)) {
                other  = true;
                other2 = true;
            } else {
                throw std::invalid_argument("The curves must share an endpoint. Constrain them "
                                            "with Constrain -> On Point before constraining "
                                            "tangent.");
            }
            return AddConstraint(CONSTRAINT::Type::CURVE_CURVE_TANGENT, workplane, 0.,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, entityA, entityB,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, other, other2);
        }
        throw std::invalid_argument("Invalid arguments for tangent constraint");
    }

    inline CONSTRAINT DistanceProj(ENTITY ptA, ENTITY ptB, double value) {
        if(ptA.IsPoint() && ptB.IsPoint()) {
            return AddConstraint(CONSTRAINT::Type::PROJ_PT_DISTANCE, ENTITY::_FREE_IN_3D,
                                 value, ptA, ptB);
        }
        throw std::invalid_argument("Invalid arguments for projected distance constraint");
    }

    inline CONSTRAINT LengthDiff(ENTITY entityA, ENTITY entityB, double value,
                                     ENTITY workplane = ENTITY::_FREE_IN_3D) {
        if(entityA.IsLine() && entityB.IsLine()) {
            return AddConstraint(CONSTRAINT::Type::LENGTH_DIFFERENCE, workplane, value,
                                 ENTITY::_NO_ENTITY, ENTITY::_NO_ENTITY, entityA, entityB);
        }
        throw std::invalid_argument("Invalid arguments for length difference constraint");
    }

    inline CONSTRAINT Dragged(ENTITY ptA, ENTITY workplane = ENTITY::_FREE_IN_3D) {
        if(ptA.IsPoint()) {
            return AddConstraint(CONSTRAINT::Type::WHERE_DRAGGED, workplane, 0., ptA);
        }
        throw std::invalid_argument("Invalid arguments for dragged constraint");
    }

    void Clear();
#ifndef LIBRARY
    BBox CalculateEntityBBox(bool includingInvisible);
    Group *GetRunningMeshGroupFor(hGroup h);
#endif
};
#undef ENTITY
#undef CONSTRAINT

class SolveSpaceUI {
public:
    TextWindow                 *pTW;
    TextWindow                 &TW;
    GraphicsWindow              GW;

    // The state for undo/redo
    typedef struct UndoState {
        IdList<Group,hGroup>            group;
        List<hGroup>                    groupOrder;
        IdList<Request,hRequest>        request;
        IdList<Constraint,hConstraint>  constraint;
        IdList<Param,hParam>            param;
        IdList<Style,hStyle>            style;
        hGroup                          activeGroup;

        void Clear() {
            group.Clear();
            request.Clear();
            constraint.Clear();
            param.Clear();
            style.Clear();
        }
    } UndoState;
    enum { MAX_UNDO = 100 };
    typedef struct {
        UndoState   d[MAX_UNDO];
        int         cnt;
        int         write;
    } UndoStack;
    UndoStack   undo;
    UndoStack   redo;

    std::map<Platform::Path, std::shared_ptr<Pixmap>, Platform::PathLess> images;
    bool ReloadLinkedImage(const Platform::Path &saveFile, Platform::Path *filename,
                           bool canCancel);

    void UndoEnableMenus();
    void UndoRemember();
    void UndoUndo();
    void UndoRedo();
    void PushFromCurrentOnto(UndoStack *uk);
    void PopOntoCurrentFrom(UndoStack *uk);
    void UndoClearState(UndoState *ut);
    void UndoClearStack(UndoStack *uk);

    // Little bits of extra configuration state
    enum { MODEL_COLORS = 8 };
    RgbaColor modelColor[MODEL_COLORS];
    Vector   lightDir[2];
    double   lightIntensity[2];
    double   ambientIntensity;
    double   chordTol;
    double   chordTolCalculated;
    int      maxSegments;
    double   exportChordTol;
    int      exportMaxSegments;
    int      timeoutRedundantConstr; //milliseconds
    double   cameraTangent;
    double   gridSpacing;
    double   exportScale;
    double   exportOffset;
    bool     fixExportColors;
    bool     exportBackgroundColor;
    bool     drawBackFaces;
    bool     showContourAreas;
    bool     checkClosedContour;
    bool     turntableNav;
    bool     immediatelyEditDimension;
    bool     automaticLineConstraints;
    bool     showToolbar;
    Platform::Path screenshotFile;
    RgbaColor backgroundColor;
    bool     exportShadedTriangles;
    bool     exportPwlCurves;
    bool     exportCanvasSizeAuto;
    bool     exportMode;
    struct {
        double  left;
        double  right;
        double  bottom;
        double  top;
    }        exportMargin;
    struct {
        double  width;
        double  height;
        double  dx;
        double  dy;
    }        exportCanvas;
    struct {
        double  depth;
        double  safeHeight;
        int     passes;
        double  feed;
        double  plungeFeed;
    }        gCode;

    Unit     viewUnits;
    int      afterDecimalMm;
    int      afterDecimalInch;
    int      afterDecimalDegree;
    bool     useSIPrefixes;
    int      autosaveInterval; // in minutes
    bool     explode;
    double   explodeDistance;

    std::string MmToString(double v, bool editable=false);
    std::string MmToStringSI(double v, int dim = 0);
    std::string DegreeToString(double v);
    double ExprToMm(Expr *e);
    double StringToMm(const std::string &s);
    const char *UnitName();
    double MmPerUnit();
    int UnitDigitsAfterDecimal();
    void SetUnitDigitsAfterDecimal(int v);
    double ChordTolMm();
    double ExportChordTolMm();
    int GetMaxSegments();
    bool usePerspectiveProj;
    double CameraTangent();

    // Some stuff relating to the tangent arcs created non-parametrically
    // as special requests.
    double tangentArcRadius;
    bool tangentArcManual;
    bool tangentArcModify;

    // The platform-dependent code calls this before entering the msg loop
    void Init();
    void Exit();

    // File load/save routines, including the additional files that get
    // loaded when we have link groups.
    FILE        *fh;
    void AfterNewFile();
    void AddToRecentList(const Platform::Path &filename);
    Platform::Path saveFile;
    bool        fileLoadError;
    bool        unsaved;
    typedef struct {
        char        type;
        const char *desc;
        char        fmt;
        void       *ptr;
    } SaveTable;
    static const SaveTable SAVED[];
    void SaveUsingTable(const Platform::Path &filename, int type);
    void LoadUsingTable(const Platform::Path &filename, char *key, char *val);
    struct {
        Group        g;
        Request      r;
        Entity       e;
        Param        p;
        Constraint   c;
        Style        s;
    } sv;
    static void MenuFile(Command id);
    void Autosave();
    void RemoveAutosave();
    static constexpr size_t MAX_RECENT = 8;
    static constexpr const char *SKETCH_EXT = "slvs";
    static constexpr const char *BACKUP_EXT = "slvs~";
    std::vector<Platform::Path> recentFiles;
    bool Load(const Platform::Path &filename);
    bool GetFilenameAndSave(bool saveAs);
    bool OkayToStartNewFile();
    hGroup CreateDefaultDrawingGroup();
    void UpdateWindowTitles();
    void ClearExisting();
    void NewFile();
    bool SaveToFile(const Platform::Path &filename);
    bool LoadAutosaveFor(const Platform::Path &filename);
    std::function<void(const Platform::Path &filename, bool is_saveAs, bool is_autosave)> OnSaveFinished;
    bool LoadFromFile(const Platform::Path &filename, bool canCancel = false);
    void UpgradeLegacyData();
    bool LoadEntitiesFromFile(const Platform::Path &filename, EntityList *le,
                              SMesh *m, SShell *sh);
    bool LoadEntitiesFromSlvs(const Platform::Path &filename, EntityList *le,
                              SMesh *m, SShell *sh);
    bool ReloadAllLinked(const Platform::Path &filename, bool canCancel = false);
    // And the various export options
    void ExportAsPngTo(const Platform::Path &filename);
    void ExportMeshTo(const Platform::Path &filename);
    void ExportMeshAsStlTo(FILE *f, SMesh *sm);
    void ExportMeshAsObjTo(FILE *fObj, FILE *fMtl, SMesh *sm);
    void ExportMeshAsThreeJsTo(FILE *f, const Platform::Path &filename,
                               SMesh *sm, SOutlineList *sol);
    void ExportMeshAsVrmlTo(FILE *f, const Platform::Path &filename, SMesh *sm);
    void ExportViewOrWireframeTo(const Platform::Path &filename, bool exportWireframe);
    void ExportSectionTo(const Platform::Path &filename);
    void ExportWireframeCurves(SEdgeList *sel, SBezierList *sbl,
                               VectorFileWriter *out);
    void ExportLinesAndMesh(SEdgeList *sel, SBezierList *sbl, SMesh *sm,
                            Vector u, Vector v,
                            Vector n, Vector origin,
                            double cameraTan,
                            VectorFileWriter *out);

    static void MenuAnalyze(Command id);

    // Additional display stuff
    struct {
        SContour    path;
        hEntity     point;
    } traced;
    SEdgeList nakedEdges;
    struct {
        bool        draw;
        Vector      ptA;
        Vector      ptB;
    } extraLine;
    struct {
        bool        draw, showOrigin;
        Vector      pt, u, v;
    } justExportedInfo;
    struct {
        bool   draw;
        bool   dirty;
        Vector position;
    } centerOfMass;

    class Clipboard {
    public:
        List<ClipboardRequest>  r;
        List<Constraint>        c;

        void Clear();
        bool ContainsEntity(hEntity old);
        hEntity NewEntityFor(hEntity old);
    };
    Clipboard clipboard;

    void MarkGroupDirty(hGroup hg, bool onlyThis = false);
    void MarkGroupDirtyByEntity(hEntity he);

    // Consistency checking on the sketch: stuff with missing dependencies
    // will get deleted automatically.
    struct {
        int     requests;
        int     groups;
        int     constraints;
        int     nonTrivialConstraints;
    } deleted;
    bool GroupExists(hGroup hg);
    bool PruneOrphans();
    bool EntityExists(hEntity he);
    bool GroupsInOrder(hGroup before, hGroup after);
    bool PruneGroups(hGroup hg);
    bool PruneRequests(hGroup hg);
    bool PruneConstraints(hGroup hg);
    static void ShowNakedEdges(bool reportOnlyWhenNotOkay);

    enum class Generate : uint32_t {
        DIRTY,
        ALL,
        REGEN,
        UNTIL_ACTIVE,
    };

    void GenerateAll(Generate type = Generate::DIRTY, bool andFindFree = false,
                     bool genForBBox = false);
    void SolveGroup(hGroup hg, bool andFindFree);
    void SolveGroupAndReport(hGroup hg, bool andFindFree);
    SolveResult TestRankForGroup(hGroup hg, int *rank = NULL);
    void WriteEqSystemForGroup(hGroup hg);
    void MarkDraggedParams();
    void ForceReferences();
    void UpdateCenterOfMass();

    bool ActiveGroupsOkay();

    // The system to be solved.
    System     *pSys;
    System     &sys;

    // All the TrueType fonts in memory
    TtfFontList fonts;

    // Everything has been pruned, so we know there's no dangling references
    // to entities that don't exist. Before that, we mustn't try to display
    // the sketch!
    bool allConsistent;

    bool scheduledGenerateAll;
    bool scheduledShowTW;
    Platform::TimerRef refreshTimer;
    Platform::TimerRef autosaveTimer;
    void Refresh();
    void ScheduleShowTW();
    void ScheduleGenerateAll();
    void ScheduleAutosave();

    static void MenuHelp(Command id);

    void Clear();

    // We allocate TW and sys on the heap to work around an MSVC problem
    // where it puts zero-initialized global data in the binary (~30M of zeroes)
    // in release builds.
    SolveSpaceUI()
        : pTW(new TextWindow()), TW(*pTW),
          pSys(new System()), sys(*pSys) {}

    ~SolveSpaceUI() {
        delete pTW;
        delete pSys;
    }
};

void ImportDxf(const Platform::Path &file);
void ImportDwg(const Platform::Path &file);
bool LinkIDF(const Platform::Path &filename, EntityList *le, SMesh *m, SShell *sh);
bool LinkStl(const Platform::Path &filename, EntityList *le, SMesh *m, SShell *sh);

extern SolveSpaceUI SS;
extern Sketch SK;

}

#ifndef __OBJC__
using namespace SolveSpace;
#endif

#endif
