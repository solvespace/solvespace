//-----------------------------------------------------------------------------
// All declarations not grouped specially elsewhere.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------

#ifndef __SOLVESPACE_H
#define __SOLVESPACE_H

// Debugging functions
#define oops() do { dbp("oops at line %d, file %s", __LINE__, __FILE__); \
                    if(0) *(char *)0 = 1; exit(-1); } while(0)
#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif

#define isnan(x) (((x) != (x)) || (x > 1e11) || (x < -1e11))

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

// Why is this faster than the library function?
inline double ffabs(double v) { return (v > 0) ? v : (-v); }

#define SWAP(T, a, b) do { T temp = (a); (a) = (b); (b) = temp; } while(0)
#define ZERO(v) memset((v), 0, sizeof(*(v)))
#define CO(v) (v).x, (v).y, (v).z

#define LENGTH_EPS      (1e-6)
#define VERY_POSITIVE   (1e10)
#define VERY_NEGATIVE   (-1e10)

#define isforname(c) (isalnum(c) || (c) == '_' || (c) == '-' || (c) == '#')

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

inline double Random(double vmax) {
    return (vmax*rand()) / RAND_MAX;
}

class Expr;
class ExprVector;
class ExprQuaternion;


//================
// From the platform-specific code.
#if !defined(WIN32)
#include <limits.h>
#define MAX_PATH PATH_MAX

#include <algorithm>
using std::min;
using std::max;
#endif

#define MAX_RECENT 8
#define RECENT_OPEN     (0xf000)
#define RECENT_IMPORT   (0xf100)
extern char RecentFile[MAX_RECENT][MAX_PATH];
void RefreshRecentMenus(void);

int SaveFileYesNoCancel(void);
// SolveSpace native file format
#define SLVS_PATTERN "SolveSpace Models (*.slvs)\0*.slvs\0All Files (*)\0*\0\0"
#define SLVS_EXT "slvs"
// PNG format bitmap
#define PNG_PATTERN "PNG (*.png)\0*.png\0All Files (*)\0*\0\0"
#define PNG_EXT "png"
// Triangle mesh
#define MESH_PATTERN "STL Mesh (*.stl)\0*.stl\0" \
                     "Wavefront OBJ Mesh (*.obj)\0*.obj\0" \
                     "All Files (*)\0*\0\0"
#define MESH_EXT "stl"
// NURBS surfaces
#define SRF_PATTERN "STEP File (*.step;*.stp)\0*.step;*.stp\0" \
                    "All Files(*)\0*\0\0"
#define SRF_EXT "step"
// 2d vector (lines and curves) format
#define VEC_PATTERN "PDF File (*.pdf)\0*.pdf\0" \
                    "Encapsulated PostScript (*.eps;*.ps)\0*.eps;*.ps\0" \
                    "Scalable Vector Graphics (*.svg)\0*.svg\0" \
                    "STEP File (*.step;*.stp)\0*.step;*.stp\0" \
                    "DXF File (*.dxf)\0*.dxf\0" \
                    "HPGL File (*.plt;*.hpgl)\0*.plt;*.hpgl\0" \
                    "G Code (*.txt)\0*.txt\0" \
                    "All Files (*)\0*\0\0"
#define VEC_EXT "pdf"
// 3d vector (wireframe lines and curves) format
#define V3D_PATTERN "STEP File (*.step;*.stp)\0*.step;*.stp\0" \
                    "DXF File (*.dxf)\0*.dxf\0" \
                    "All Files (*)\0*\0\0"
#define V3D_EXT "step"
// Comma-separated value, like a spreadsheet would use
#define CSV_PATTERN "CSV File (*.csv)\0*.csv\0All Files (*)\0*\0\0"
#define CSV_EXT "csv"
bool GetSaveFile(char *file, char *defExtension, char *selPattern);
bool GetOpenFile(char *file, char *defExtension, char *selPattern);
void GetAbsoluteFilename(char *file);
void LoadAllFontFiles(void);

void OpenWebsite(char *url);

void CheckMenuById(int id, bool checked);
void EnableMenuById(int id, bool checked);

void ShowGraphicsEditControl(int x, int y, char *s);
void HideGraphicsEditControl(void);
bool GraphicsEditControlIsVisible(void);
void ShowTextEditControl(int x, int y, char *s);
void HideTextEditControl(void);
bool TextEditControlIsVisible(void);
void MoveTextScrollbarTo(int pos, int maxPos, int page);

#define CONTEXT_SUBMENU     (-1)
#define CONTEXT_SEPARATOR   (-2)
void AddContextMenuItem(char *legend, int id);
void CreateContextSubmenu(void);
int ShowContextMenu(void);

void ShowTextWindow(bool visible);
void InvalidateText(void);
void InvalidateGraphics(void);
void PaintGraphics(void);
void GetGraphicsWindowSize(int *w, int *h);
void GetTextWindowSize(int *w, int *h);
int32_t GetMilliseconds(void);
int64_t GetUnixTime(void);

void dbp(const char *str, ...);
#define DBPTRI(tri) \
    dbp("tri: (%.3f %.3f %.3f) (%.3f %.3f %.3f) (%.3f %.3f %.3f)", \
        CO((tri).a), CO((tri).b), CO((tri).c))

void SetWindowTitle(char *str);
void SetMousePointerToHand(bool yes);
void DoMessageBox(char *str, int rows, int cols, bool error);
void SetTimerFor(int milliseconds);
void ExitNow(void);

void CnfFreezeString(char *str, char *name);
void CnfFreezeuint32_t(uint32_t v, char *name);
void CnfFreezeFloat(float v, char *name);
void CnfThawString(char *str, int maxLen, char *name);
uint32_t CnfThawuint32_t(uint32_t v, char *name);
float CnfThawFloat(float v, char *name);

void *AllocTemporary(int n);
void FreeTemporary(void *p);
void FreeAllTemporary(void);
void *MemRealloc(void *p, int n);
void *MemAlloc(int n);
void MemFree(void *p);
void InitHeaps(void);
void vl(void); // debug function to validate heaps

// End of platform-specific functions
//================

class Group;
class SSurface;
#include "dsc.h"
#include "polygon.h"
#include "srf/surface.h"

class Entity;
class hEntity;
class Param;
class hParam;
typedef IdList<Entity,hEntity> EntityList;
typedef IdList<Param,hParam> ParamList;

#include "sketch.h"
#include "ui.h"
#include "expr.h"


// Utility functions that are provided in the platform-independent code.
void glxVertex3v(Vector u);
void glxAxisAlignedQuad(double l, double r, double t, double b);
void glxAxisAlignedLineLoop(double l, double r, double t, double b);
#define DEFAULT_TEXT_HEIGHT (11.5)
#if defined(WIN32)
#define GLX_CALLBACK __stdcall
#else
#define GLX_CALLBACK
#endif
typedef void GLX_CALLBACK glxCallbackFptr(void);
struct GLUtesselator;
void glxTesselatePolygon(GLUtesselator *gt, SPolygon *p);
void glxFillPolygon(SPolygon *p);
void glxFillMesh(int color, SMesh *m, uint32_t h, uint32_t s1, uint32_t s2);
void glxDebugPolygon(SPolygon *p);
void glxDrawEdges(SEdgeList *l, bool endpointsToo);
void glxDebugMesh(SMesh *m);
void glxMarkPolygonNormal(SPolygon *p);
typedef void glxLineFn(void *data, Vector a, Vector b);
void glxWriteText(char *str, double h, Vector t, Vector u, Vector v,
    glxLineFn *fn, void *fndata);
void glxWriteTextRefCenter(char *str, double h, Vector t, Vector u, Vector v,
    glxLineFn *fn, void *fndata);
double glxStrWidth(char *str, double h);
double glxStrHeight(double h);
void glxLockColorTo(uint32_t rgb);
void glxFatLine(Vector a, Vector b, double width);
void glxUnlockColor(void);
void glxColorRGB(uint32_t rgb);
void glxColorRGBa(uint32_t rgb, double a);
void glxDepthRangeOffset(int units);
void glxDepthRangeLockToFront(bool yes);
void glxDrawPixelsWithTexture(uint8_t *data, int w, int h);
void glxCreateBitmapFont(void);
void glxBitmapText(char *str, Vector p);
void glxBitmapCharQuad(char c, double x, double y);
#define TEXTURE_BACKGROUND_IMG  10
#define TEXTURE_BITMAP_FONT     20
#define TEXTURE_DRAW_PIXELS     30
#define TEXTURE_COLOR_PICKER_2D 40
#define TEXTURE_COLOR_PICKER_1D 50


#define arraylen(x) (sizeof((x))/sizeof((x)[0]))
#define PI (3.1415926535897931)
void MakeMatrix(double *mat, double a11, double a12, double a13, double a14,
                             double a21, double a22, double a23, double a24,
                             double a31, double a32, double a33, double a34,
                             double a41, double a42, double a43, double a44);
void MakePathRelative(const char *base, char *path);
void MakePathAbsolute(const char *base, char *path);
bool StringAllPrintable(const char *str);
bool StringEndsIn(const char *str, const char *ending);
void Message(const char *str, ...);
void Error(const char *str, ...);

class System {
public:
    static const int MAX_UNKNOWNS      = 1024;

    EntityList                      entity;
    ParamList                       param;
    IdList<Equation,hEquation>      eq;

    // A list of parameters that are being dragged; these are the ones that
    // we should put as close as possible to their initial positions.
    List<hParam>                    dragged;

    // In general, the tag indicates the subsys that a variable/equation
    // has been assigned to; these are exceptions for variables:
    static const int VAR_SUBSTITUTED      = 10000;
    static const int VAR_DOF_TEST         = 10001;
    // and for equations:
    static const int EQ_SUBSTITUTED       = 20000;

    // The system Jacobian matrix
    struct {
        // The corresponding equation for each row
        hEquation   eq[MAX_UNKNOWNS];

        // The corresponding parameter for each column
        hParam      param[MAX_UNKNOWNS];

        // We're solving AX = B
        int m, n;
        struct {
            Expr        *sym[MAX_UNKNOWNS][MAX_UNKNOWNS];
            double       num[MAX_UNKNOWNS][MAX_UNKNOWNS];
        }           A;

        double      scale[MAX_UNKNOWNS];

        // Some helpers for the least squares solve
        double AAt[MAX_UNKNOWNS][MAX_UNKNOWNS];
        double Z[MAX_UNKNOWNS];

        double      X[MAX_UNKNOWNS];

        struct {
            Expr        *sym[MAX_UNKNOWNS];
            double       num[MAX_UNKNOWNS];
        }           B;
    } mat;

    static const double RANK_MAG_TOLERANCE, CONVERGE_TOLERANCE;
    int CalculateRank(void);
    bool TestRank(void);
    static bool SolveLinearSystem(double X[], double A[][MAX_UNKNOWNS],
                                  double B[], int N);
    bool SolveLeastSquares(void);

    bool WriteJacobian(int tag);
    void EvalJacobian(void);

    void WriteEquationsExceptFor(hConstraint hc, Group *g);
    void FindWhichToRemoveToFixJacobian(Group *g, List<hConstraint> *bad);
    void SolveBySubstitution(void);

    bool IsDragged(hParam p);

    bool NewtonSolve(int tag);

    enum {
        SOLVED_OKAY              = 0,
        DIDNT_CONVERGE           = 10,
        REDUNDANT_OKAY           = 11,
        REDUNDANT_DIDNT_CONVERGE = 12,
        TOO_MANY_UNKNOWNS        = 20
    };
    int Solve(Group *g, int *dof, List<hConstraint> *bad,
                bool andFindBad, bool andFindFree);
};

class TtfFont {
public:
    typedef struct {
        bool        onCurve;
        bool        lastInContour;
        int16_t       x;
        int16_t       y;
    } FontPoint;

    typedef struct {
        FontPoint   *pt;
        int         pts;

        int         xMax;
        int         xMin;
        int         leftSideBearing;
        int         advanceWidth;
    } Glyph;

    typedef struct {
        int x, y;
    } IntPoint;

    char    fontFile[MAX_PATH];
    NameStr name;
    bool    loaded;

    // The font itself, plus the mapping from ASCII codes to glyphs
    int     useGlyph[256];
    Glyph   *glyph;
    int     glyphs;

    int     maxPoints;
    int     scale;

    // The filehandle, while loading
    FILE   *fh;
    // Some state while rendering a character to curves
    static const int NOTHING   = 0;
    static const int ON_CURVE  = 1;
    static const int OFF_CURVE = 2;
    int         lastWas;
    IntPoint    lastOnCurve;
    IntPoint    lastOffCurve;

    // And the state that the caller must specify, determines where we
    // render to and how
    SBezierList *beziers;
    Vector      origin, u, v;

    int Getc(void);
    int Getuint8_t(void);
    int Getuint16_t(void);
    int Getuint32_t(void);

    void LoadGlyph(int index);
    bool LoadFontFromFile(bool nameOnly);
    char *FontFileBaseName(void);

    void Flush(void);
    void Handle(int *dx, int x, int y, bool onCurve);
    void PlotCharacter(int *dx, int c, double spacing);
    void PlotString(char *str, double spacing,
                    SBezierList *sbl, Vector origin, Vector u, Vector v);

    Vector TransformIntPoint(int x, int y);
    void LineSegment(int x0, int y0, int x1, int y1);
    void Bezier(int x0, int y0, int x1, int y1, int x2, int y2);
};

class TtfFontList {
public:
    bool                loaded;
    List<TtfFont>       l;

    void LoadAll(void);

    void PlotString(char *font, char *str, double spacing,
                    SBezierList *sbl, Vector origin, Vector u, Vector v);
};

class StepFileWriter {
public:
    void ExportSurfacesTo(char *filename);
    void WriteHeader(void);
    int ExportCurve(SBezier *sb);
    int ExportCurveLoop(SBezierLoop *loop, bool inner);
    void ExportSurface(SSurface *ss, SBezierList *sbl);
    void WriteWireframe(void);
    void WriteFooter(void);

    List<int> curves;
    List<int> advancedFaces;
    FILE *f;
    int id;
};

class VectorFileWriter {
public:
    FILE *f;
    Vector ptMin, ptMax;

    static double MmToPts(double mm);

    static VectorFileWriter *ForFile(char *file);

    void Output(SBezierLoopSetSet *sblss, SMesh *sm);

    void BezierAsPwl(SBezier *sb);
    void BezierAsNonrationalCubic(SBezier *sb, int depth=0);

    virtual void StartPath( uint32_t strokeRgb, double lineWidth,
                            bool filled, uint32_t fillRgb) = 0;
    virtual void FinishPath(uint32_t strokeRgb, double lineWidth,
                            bool filled, uint32_t fillRgb) = 0;
    virtual void Bezier(SBezier *sb) = 0;
    virtual void Triangle(STriangle *tr) = 0;
    virtual void StartFile(void) = 0;
    virtual void FinishAndCloseFile(void) = 0;
    virtual bool HasCanvasSize(void) = 0;
};
class DxfFileWriter : public VectorFileWriter {
public:
    void StartPath( uint32_t strokeRgb, double lineWidth,
                    bool filled, uint32_t fillRgb);
    void FinishPath(uint32_t strokeRgb, double lineWidth,
                    bool filled, uint32_t fillRgb);
    void Triangle(STriangle *tr);
    void Bezier(SBezier *sb);
    void StartFile(void);
    void FinishAndCloseFile(void);
    bool HasCanvasSize(void) { return false; }
};
class EpsFileWriter : public VectorFileWriter {
public:
    Vector prevPt;
    void MaybeMoveTo(Vector s, Vector f);

    void StartPath( uint32_t strokeRgb, double lineWidth,
                    bool filled, uint32_t fillRgb);
    void FinishPath(uint32_t strokeRgb, double lineWidth,
                    bool filled, uint32_t fillRgb);
    void Triangle(STriangle *tr);
    void Bezier(SBezier *sb);
    void StartFile(void);
    void FinishAndCloseFile(void);
    bool HasCanvasSize(void) { return true; }
};
class PdfFileWriter : public VectorFileWriter {
public:
    uint32_t xref[10];
    uint32_t bodyStart;
    Vector prevPt;
    void MaybeMoveTo(Vector s, Vector f);

    void StartPath( uint32_t strokeRgb, double lineWidth,
                    bool filled, uint32_t fillRgb);
    void FinishPath(uint32_t strokeRgb, double lineWidth,
                    bool filled, uint32_t fillRgb);
    void Triangle(STriangle *tr);
    void Bezier(SBezier *sb);
    void StartFile(void);
    void FinishAndCloseFile(void);
    bool HasCanvasSize(void) { return true; }
};
class SvgFileWriter : public VectorFileWriter {
public:
    Vector prevPt;
    void MaybeMoveTo(Vector s, Vector f);

    void StartPath( uint32_t strokeRgb, double lineWidth,
                    bool filled, uint32_t fillRgb);
    void FinishPath(uint32_t strokeRgb, double lineWidth,
                    bool filled, uint32_t fillRgb);
    void Triangle(STriangle *tr);
    void Bezier(SBezier *sb);
    void StartFile(void);
    void FinishAndCloseFile(void);
    bool HasCanvasSize(void) { return true; }
};
class HpglFileWriter : public VectorFileWriter {
public:
    static double MmToHpglUnits(double mm);
    void StartPath( uint32_t strokeRgb, double lineWidth,
                    bool filled, uint32_t fillRgb);
    void FinishPath(uint32_t strokeRgb, double lineWidth,
                    bool filled, uint32_t fillRgb);
    void Triangle(STriangle *tr);
    void Bezier(SBezier *sb);
    void StartFile(void);
    void FinishAndCloseFile(void);
    bool HasCanvasSize(void) { return false; }
};
class Step2dFileWriter : public VectorFileWriter {
    StepFileWriter sfw;
    void StartPath( uint32_t strokeRgb, double lineWidth,
                    bool filled, uint32_t fillRgb);
    void FinishPath(uint32_t strokeRgb, double lineWidth,
                    bool filled, uint32_t fillRgb);
    void Triangle(STriangle *tr);
    void Bezier(SBezier *sb);
    void StartFile(void);
    void FinishAndCloseFile(void);
    bool HasCanvasSize(void) { return false; }
};
class GCodeFileWriter : public VectorFileWriter {
public:
    SEdgeList sel;
    void StartPath( uint32_t strokeRgb, double lineWidth,
                    bool filled, uint32_t fillRgb);
    void FinishPath(uint32_t strokeRgb, double lineWidth,
                    bool filled, uint32_t fillRgb);
    void Triangle(STriangle *tr);
    void Bezier(SBezier *sb);
    void StartFile(void);
    void FinishAndCloseFile(void);
    bool HasCanvasSize(void) { return false; }
};

#ifdef LIBRARY
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
    inline Group   *GetGroup  (hGroup   h) { return group.  FindById(h); }
    // Styles are handled a bit differently.
};
#undef ENTITY
#undef CONSTRAINT

class SolveSpace {
public:
    TextWindow                  TW;
    GraphicsWindow              GW;

    // The state for undo/redo
    typedef struct {
        IdList<Group,hGroup>            group;
        IdList<Request,hRequest>        request;
        IdList<Constraint,hConstraint>  constraint;
        IdList<Param,hParam>            param;
        IdList<Style,hStyle>            style;
        hGroup                          activeGroup;
    } UndoState;
    static const int MAX_UNDO = 16;
    typedef struct {
        UndoState   d[MAX_UNDO];
        int         cnt;
        int         write;
    } UndoStack;
    UndoStack   undo;
    UndoStack   redo;
    void UndoEnableMenus(void);
    void UndoRemember(void);
    void UndoUndo(void);
    void UndoRedo(void);
    void PushFromCurrentOnto(UndoStack *uk);
    void PopOntoCurrentFrom(UndoStack *uk);
    void UndoClearState(UndoState *ut);
    void UndoClearStack(UndoStack *uk);

    // Little bits of extra configuration state
    static const int MODEL_COLORS = 8;
    int     modelColor[MODEL_COLORS];
    Vector  lightDir[2];
    double  lightIntensity[2];
    double  ambientIntensity;
    double  chordTol;
    int     maxSegments;
    double  cameraTangent;
    float   gridSpacing;
    float   exportScale;
    float   exportOffset;
    int     fixExportColors;
    int     drawBackFaces;
    int     checkClosedContour;
    int     showToolbar;
    uint32_t   backgroundColor;
    int     exportShadedTriangles;
    int     exportPwlCurves;
    int     exportCanvasSizeAuto;
    struct {
        float   left;
        float   right;
        float   bottom;
        float   top;
    }       exportMargin;
    struct {
        float   width;
        float   height;
        float   dx;
        float   dy;
    }       exportCanvas;
    struct {
        float   depth;
        int     passes;
        float   feed;
        float   plungeFeed;
    }       gCode;

    typedef enum {
        UNIT_MM = 0,
        UNIT_INCHES,
    } Unit;
    Unit    viewUnits;
    int     afterDecimalMm;
    int     afterDecimalInch;

    char *MmToString(double v);
    double ExprToMm(Expr *e);
    double StringToMm(char *s);
    char *UnitName(void);
    double MmPerUnit(void);
    int UnitDigitsAfterDecimal(void);
    void SetUnitDigitsAfterDecimal(int v);
    double ChordTolMm(void);
    bool usePerspectiveProj;
    double CameraTangent(void);

    // Some stuff relating to the tangent arcs created non-parametrically
    // as special requests.
    double tangentArcRadius;
    bool tangentArcManual;
    bool tangentArcDeleteOld;

    // The platform-dependent code calls this before entering the msg loop
    void Init(char *cmdLine);
    void Exit(void);

    // File load/save routines, including the additional files that get
    // loaded when we have import groups.
    FILE        *fh;
    void AfterNewFile(void);
    static void RemoveFromRecentList(char *file);
    static void AddToRecentList(char *file);
    char saveFile[MAX_PATH];
    bool fileLoadError;
    bool unsaved;
    typedef struct {
        char     type;
        char    *desc;
        char     fmt;
        void    *ptr;
    } SaveTable;
    static const SaveTable SAVED[];
    void SaveUsingTable(int type);
    void LoadUsingTable(char *key, char *val);
    struct {
        Group        g;
        Request      r;
        Entity       e;
        Param        p;
        Constraint   c;
        Style        s;
    } sv;
    static void MenuFile(int id);
    bool GetFilenameAndSave(bool saveAs);
    bool OkayToStartNewFile(void);
    hGroup CreateDefaultDrawingGroup(void);
    void UpdateWindowTitle(void);
    void ClearExisting(void);
    void NewFile(void);
    bool SaveToFile(char *filename);
    bool LoadFromFile(char *filename);
    bool LoadEntitiesFromFile(char *filename, EntityList *le,
                                SMesh *m, SShell *sh);
    void ReloadAllImported(void);
    // And the various export options
    void ExportAsPngTo(char *file);
    void ExportMeshTo(char *file);
    void ExportMeshAsStlTo(FILE *f, SMesh *sm);
    void ExportMeshAsObjTo(FILE *f, SMesh *sm);
    void ExportViewOrWireframeTo(char *file, bool wireframe);
    void ExportSectionTo(char *file);
    void ExportWireframeCurves(SEdgeList *sel, SBezierList *sbl,
                               VectorFileWriter *out);
    void ExportLinesAndMesh(SEdgeList *sel, SBezierList *sbl, SMesh *sm,
                            Vector u, Vector v, Vector n, Vector origin,
                                double cameraTan,
                            VectorFileWriter *out);

    static void MenuAnalyze(int id);

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
        uint8_t        *fromFile;
        int         w, h;
        int         rw, rh;
        double      scale; // pixels per mm
        Vector      origin;
    } bgImage;
    struct {
        bool        draw;
        Vector      pt, u, v;
    } justExportedInfo;

    class Clipboard {
public:
        List<ClipboardRequest>  r;
        List<Constraint>        c;

        void Clear(void);
        bool ContainsEntity(hEntity old);
        hEntity NewEntityFor(hEntity old);
    };
    Clipboard clipboard;

    void MarkGroupDirty(hGroup hg);
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
    bool PruneOrphans(void);
    bool EntityExists(hEntity he);
    bool GroupsInOrder(hGroup before, hGroup after);
    bool PruneGroups(hGroup hg);
    bool PruneRequests(hGroup hg);
    bool PruneConstraints(hGroup hg);

    void GenerateAll(void);
    void GenerateAll(int first, int last, bool andFindFree=false);
    void SolveGroup(hGroup hg, bool andFindFree);
    void MarkDraggedParams(void);
    void ForceReferences(void);

    bool AllGroupsOkay(void);

    // The system to be solved.
    System  sys;

    // All the TrueType fonts in memory
    TtfFontList fonts;

    // Everything has been pruned, so we know there's no dangling references
    // to entities that don't exist. Before that, we mustn't try to display
    // the sketch!
    bool allConsistent;

    struct {
        bool    showTW;
        bool    generateAll;
    } later;
    void DoLater(void);

    static void MenuHelp(int id);
};

extern SolveSpace SS;
extern Sketch SK;

#endif
