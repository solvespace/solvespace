//-----------------------------------------------------------------------------
// All declarations not grouped specially elsewhere.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_H
#define SOLVESPACE_H

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>

#define EIGEN_NO_DEBUG
#undef Success
#include <Eigen/SparseCore>

#include "dsc.h"
#include "polygon.h"
#include "srf/surface.h"
#include "expr.h"
#include "sketch.h"
#include "resource.h"
#include "ttf.h"

#include "platform/platform.h"

#ifndef SOLVESPACE_CORE_ONLY
#include "render/render.h"
#include "ui.h"
#else
// Stub i18n functions for core-only builds (originals in ui.h)
inline const char *_(const char *msgid) { return msgid; }
inline const char *C_(const char *, const char *msgid) { return msgid; }
inline const char *N_(const char *msgid) { return msgid; }
inline const char *CN_(const char *, const char *msgid) { return msgid; }
#endif

namespace SolveSpace {

using std::min;
using std::max;
using std::swap;
using std::fabs;

enum class Unit : uint32_t {
    MM = 0,
    INCHES,
    METERS,
    FEET_INCHES
};

class Pixmap;

enum class SolveResult : uint32_t {
    OKAY                     = 0,
    DIDNT_CONVERGE           = 10,
    REDUNDANT_OKAY           = 11,
    REDUNDANT_DIDNT_CONVERGE = 12,
    TOO_MANY_UNKNOWNS        = 20
};

// Utility functions that are provided in the platform-independent code.
class utf8_iterator {
    const char *p, *n;
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = char32_t;
    using difference_type = std::ptrdiff_t;
    using pointer = char32_t*;
    using reference = char32_t&;

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

class System {
public:
    enum { MAX_UNKNOWNS = 2048 };

    EntityList                      entity;
    ParamList                       param;
    IdList<Equation,hEquation>      eq;

    // A list of parameters that are being dragged; these are the ones that
    // we should put as close as possible to their initial positions.
    ParamSet                        dragged;

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

        Eigen::VectorXd X;

        struct {
            // This only observes the Expr - does not own them!
            std::vector<Expr *> sym;
            Eigen::VectorXd     num;
        } B;
    } mat;

    static const double CONVERGE_TOLERANCE;
    int CalculateRank();
    bool TestRank(int *dof = NULL, int *rank = NULL);
    static bool SolveLinearSystem(const Eigen::SparseMatrix<double> &A,
                                  const Eigen::VectorXd &B, Eigen::VectorXd *X);
    bool SolveLeastSquares();

    bool WriteJacobian(int tag);
    void EvalJacobian();

    void WriteEquationsExceptFor(hConstraint hc, Group *g);
    void FindWhichToRemoveToFixJacobian(Group *g, List<hConstraint> *bad,
                                        bool forceDofCheck);
    SubstitutionMap SolveBySubstitution();

    bool IsDragged(hParam p);

    bool NewtonSolve();

    void MarkParamsFree(bool findFree);

    SolveResult Solve(Group *g, int *dof = NULL, List<hConstraint> *bad = NULL,
                      bool andFindBad = false, bool andFindFree = false,
                      bool forceDofCheck = false);

    SolveResult SolveRank(Group *g, int *rank = NULL, int *dof = NULL,
                          List<hConstraint> *bad = NULL,
                          bool andFindBad = false, bool andFindFree = false);

    void Clear();
};

class StepFileWriter {
public:
    bool HasCartesianPointAnAlias(int number, Vector v, int vertex,
                                  bool *vertex_has_alias = nullptr);
    int InsertPoint(int number);
    int InsertVertex(int number);
    bool HasBSplineCurveAnAlias(int number, std::vector<int> points);
    int InsertCurve(int number);
    bool HasEdgeCurveAnAlias(int number, int prevFinish, int thisFinish, int curveId,
                        bool *flip = nullptr);
    int InsertEdgeCurve(int number);
    bool HasOrientedEdgeAnAlias(int number, int edgeCurveId, bool flip);
    int InsertOrientedEdge(int number);
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

    // Structs to keep track of duplicated entities.
    // Basic alias.
    typedef struct {
        int reference;
        std::vector<int> aliases;
    } alias_t;

    // Cartesian points.
    typedef struct {
        alias_t alias;
        alias_t vertexAlias;
        Vector v;
    } pointAliases_t;

    // Curves.
    typedef struct {
        alias_t alias;
        std::vector<int> memberPoints;
        RgbaColor color;
    } curveAliases_t;

    // Edges.
    typedef struct {
        alias_t alias;
        int prevFinish;
        int thisFinish;
        int curveId;
        RgbaColor color;
    } edgeCurveAliases_t;

    // Edges.
    typedef struct {
        alias_t alias;
        int edgeCurveId;
        bool flip;
    } orientedEdgeAliases_t;

    std::vector<pointAliases_t> pointAliases;
    std::vector<edgeCurveAliases_t> edgeCurveAliases;
    std::vector<curveAliases_t> curveAliases;
    std::vector<orientedEdgeAliases_t> orientedEdgeAliases;
    bool exportParts = true;
    RgbaColor currentColor;
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
    virtual bool OutputConstraints(IdList<ConstraintBase,hConstraint> *) { return false; }
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
    IdList<ConstraintBase,hConstraint> *constraint;

    static const char *lineTypeName(StipplePattern stippleType);

    bool OutputConstraints(IdList<ConstraintBase,hConstraint> *constraint) override;

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
    bool NeedToOutput(ConstraintBase *c);
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

#if defined(LIBRARY) || defined(SOLVESPACE_CORE_ONLY)
#   define ENTITY EntityBase
#   define CONSTRAINT ConstraintBase
#else
#   define ENTITY Entity
#   define CONSTRAINT Constraint
#endif
class Sketch {
public:
    // These are user-editable, and define the sketch.
    IdList<Group,hGroup>            group;
    List<hGroup>                    groupOrder;
    IdList<CONSTRAINT,hConstraint>  constraint;
    IdList<Request,hRequest>        request;
    IdList<Style,hStyle>            style;

    // These are generated from the above.
    IdList<ENTITY,hEntity>          entity;
    ParamList                       param;

    inline CONSTRAINT *GetConstraint(hConstraint h)
        { return constraint.FindById(h); }
    inline ENTITY  *GetEntity (hEntity  h) { return entity. FindById(h); }
    inline Param   *GetParam  (hParam   h) { return param.  FindById(h); }
    inline Request *GetRequest(hRequest h) { return request.FindById(h); }
    inline Group   *GetGroup  (hGroup   h) { return group.  FindById(h); }
    // Styles are handled a bit differently.

    void Clear();

    BBox CalculateEntityBBox(bool includingInvisible);
    Group *GetRunningMeshGroupFor(hGroup h);
};
#undef ENTITY
#undef CONSTRAINT

// Core engine class: computation, file I/O, solving - no GUI dependencies.
class SolveSpaceCore {
public:
    // The state for undo/redo
    typedef struct UndoState {
        IdList<Group,hGroup>            group;
        List<hGroup>                    groupOrder;
        IdList<Request,hRequest>        request;
        IdList<ConstraintBase,hConstraint>  constraint;
        ParamList                       param;
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

    void UndoRemember();
    void UndoUndo();
    void UndoRedo();
    void PushFromCurrentOnto(UndoStack *uk);
    void PopOntoCurrentFrom(UndoStack *uk);
    void UndoClearState(UndoState *ut);
    void UndoClearStack(UndoStack *uk);

    // Configuration state
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
    bool     arcDimDefaultDiameter;
    bool     fixExportColors;
    bool     exportBackgroundColor;
    bool     drawBackFaces;
    bool     showContourAreas;
    bool     checkClosedContour;
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

    // Image cache (used by Request::Generate for aspect ratio)
    std::map<Platform::Path, std::shared_ptr<Pixmap>, Platform::PathLess> images;

    // Tangent arc settings
    double tangentArcRadius;
    bool tangentArcManual;
    bool tangentArcModify;

    // Active editing state (mirrored from GraphicsWindow in UI builds)
    hGroup  activeGroup;

    // Viewport state needed by core export/constraint code
    Vector  projRight;
    Vector  projUp;
    double  viewScale;
    Vector  viewOffset;

    // Display toggles needed by core mesh/export code
    bool     showEdges;
    bool     showOutlines;
    bool     showShaded;
    bool     showFaces;
    bool     showFacesDrawing;
    bool     showFacesNonDrawing;
    bool     showMesh;

    enum class ShowConstraintMode : uint32_t {
        SCM_NO_CONSTRAINT = 0,
        SCM_SHOW_ALL      = 1,
        SCM_SHOW_DIM      = 2,
    };
    ShowConstraintMode showConstraints;

    enum class DrawOccludedAs : uint32_t {
        INVISIBLE = 0,
        STIPPLED  = 1,
        VISIBLE   = 2,
    };
    DrawOccludedAs drawOccludedAs;

    // File load/save
    FILE        *fh;
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
        EntityBase       e;
        Param            p;
        ConstraintBase   c;
        Style        s;
    } sv;
    static constexpr size_t MAX_RECENT = 8;
    static constexpr const char *SKETCH_EXT = "slvs";
    static constexpr const char *BACKUP_EXT = "slvs~";
    hGroup CreateDefaultDrawingGroup();
    bool SaveToFile(const Platform::Path &filename);
    bool LoadAutosaveFor(const Platform::Path &filename);
    bool LoadFromFile(const Platform::Path &filename, bool canCancel = false);
    void UpgradeLegacyData();
    bool LoadEntitiesFromFile(const Platform::Path &filename, EntityList *le,
                              SMesh *m, SShell *sh);
    bool LoadEntitiesFromSlvs(const Platform::Path &filename, EntityList *le,
                              SMesh *m, SShell *sh);
    bool ReloadAllLinked(const Platform::Path &filename, bool canCancel = false);

#ifndef SOLVESPACE_CORE_ONLY
    // Export (requires Canvas/rendering - UI builds only)
    void ExportMeshAsStlTo(FILE *f, SMesh *sm);
    void ExportMeshAsObjTo(FILE *fObj, FILE *fMtl, SMesh *sm);
    void ExportMeshAsThreeJsTo(FILE *f, const Platform::Path &filename,
                               SMesh *sm, SOutlineList *sol);
    void ExportMeshAsVrmlTo(FILE *f, const Platform::Path &filename, SMesh *sm);
    void ExportWireframeCurves(SEdgeList *sel, SBezierList *sbl,
                               VectorFileWriter *out);
    void ExportLinesAndMesh(SEdgeList *sel, SBezierList *sbl, SMesh *sm,
                            Vector u, Vector v,
                            Vector n, Vector origin,
                            double cameraTan,
                            VectorFileWriter *out);
#endif

    // Center of mass
    struct {
        bool   draw;
        bool   dirty;
        Vector position;
    } centerOfMass;

    void MarkGroupDirty(hGroup hg, bool onlyThis = false);
    void MarkGroupDirtyByEntity(hEntity he);

    // Consistency checking
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
    bool PruneRequestsAndConstraints(hGroup hg);

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

    // The system to be solved
    System     *pSys;
    System     &sys;

    // All the TrueType fonts in memory
    TtfFontList fonts;

    bool allConsistent;
    bool persistentDirty;

    // Virtual methods for UI callbacks - overridden by SolveSpaceUI
    virtual void ScheduleGenerateAll() {}
    virtual void ScheduleShowTW() {}
    virtual void Refresh() {}
    virtual void UndoEnableMenus() {}
    virtual bool ReloadLinkedImage(const Platform::Path &saveFile,
                                   Platform::Path *filename,
                                   bool canCancel) { return false; }
    virtual void UpdateWindowTitles() {}
    void ClearExisting();
    void NewFile();
    virtual void AfterNewFile() {}
    virtual void Clear();

    SolveSpaceCore()
        : pSys(new System()), sys(*pSys) {}

    virtual ~SolveSpaceCore() {
        delete pSys;
    }
};

#ifndef SOLVESPACE_CORE_ONLY

class SolveSpaceUI : public SolveSpaceCore {
public:
    TextWindow                 *pTW;
    TextWindow                 &TW;
    GraphicsWindow              GW;

    bool ReloadLinkedImage(const Platform::Path &saveFile, Platform::Path *filename,
                           bool canCancel) override;

    void UndoEnableMenus() override;

    // UI-only configuration
    int      animationSpeed; //milliseconds
    bool     showFullFilePath;
    bool     cameraNav;
    bool     turntableNav;
    bool     immediatelyEditDimension;
    bool     automaticLineConstraints;
    bool     showToolbar;
    Platform::Path screenshotFile;

    void Init();
    void Exit();

    void AfterNewFile() override;
    void AddToRecentList(const Platform::Path &filename);
    std::vector<Platform::Path> recentFiles;
    bool Load(const Platform::Path &filename);
    bool GetFilenameAndSave(bool saveAs);
    bool OkayToStartNewFile();
    void UpdateWindowTitles() override;
    std::function<void(const Platform::Path &filename, bool is_saveAs, bool is_autosave)> OnSaveFinished;
    void Autosave();
    void RemoveAutosave();

    static void MenuFile(Command id);
    static void MenuAnalyze(Command id);
    static void MenuHelp(Command id);

    // Viewport-dependent exports
    void ExportAsPngTo(const Platform::Path &filename);
    void ExportMeshTo(const Platform::Path &filename);
    void ExportViewOrWireframeTo(const Platform::Path &filename, bool exportWireframe);
    void ExportSectionTo(const Platform::Path &filename);

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

    class Clipboard {
    public:
        List<ClipboardRequest>  r;
        List<Constraint>        c;

        void Clear();
        bool ContainsEntity(hEntity old);
        hEntity NewEntityFor(hEntity old);
    };
    Clipboard clipboard;

    static void ShowNakedEdges(bool reportOnlyWhenNotOkay);

    bool scheduledGenerateAll;
    bool scheduledShowTW;
    Platform::TimerRef refreshTimer;
    Platform::TimerRef autosaveTimer;
    void Refresh() override;
    void ScheduleShowTW() override;
    void ScheduleGenerateAll() override;
    void ScheduleAutosave();

    void Clear() override;

    SolveSpaceUI()
        : SolveSpaceCore(),
          pTW(new TextWindow()), TW(*pTW) {}

    ~SolveSpaceUI() {
        delete pTW;
    }
};

#endif // !SOLVESPACE_CORE_ONLY

void ImportDxf(const Platform::Path &file);
void ImportDwg(const Platform::Path &file);
bool LinkIDF(const Platform::Path &filename, EntityList *le, SMesh *m, SShell *sh);
bool LinkStl(const Platform::Path &filename, EntityList *le, SMesh *m, SShell *sh);

#ifdef SOLVESPACE_CORE_ONLY
extern SolveSpaceCore SS;
#else
extern SolveSpaceUI SS;
#endif
extern Sketch SK;

} // namespace SolveSpace

#endif
