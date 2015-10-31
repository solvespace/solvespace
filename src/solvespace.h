//-----------------------------------------------------------------------------
// All declarations not grouped specially elsewhere.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------

#ifndef __SOLVESPACE_H
#define __SOLVESPACE_H

#include <config.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <math.h>
#include <limits.h>
#include <algorithm>
#include <string>
#include <vector>
#ifdef HAVE_STDINT_H
#   include <stdint.h>
#endif
#ifdef WIN32
#   include <windows.h> // required by GL headers
#endif
#ifdef __APPLE__
#   include <strings.h> // for strcasecmp in file.cpp
#   include <OpenGL/gl.h>
#   include <OpenGL/glu.h>
#else
#   include <GL/gl.h>
#   include <GL/glu.h>
#endif

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
#ifdef NDEBUG
#define oops() do { dbp("oops at line %d, file %s\n", __LINE__, __FILE__); \
                    exit(-1); } while(0)
#else
#define oops() do { dbp("oops at line %d, file %s\n", __LINE__, __FILE__); \
                    abort(); } while(0)
#endif

#ifndef isnan
#   define isnan(x) (((x) != (x)) || (x > 1e11) || (x < -1e11))
#endif

namespace SolveSpace {

using std::min;
using std::max;
using std::swap;

#if defined(__GNUC__)
__attribute__((__format__ (__printf__, 1, 2)))
#endif
std::string ssprintf(const char *fmt, ...);

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

#define CO(v) (v).x, (v).y, (v).z

#define ANGLE_COS_EPS   (1e-6)
#define LENGTH_EPS      (1e-6)
#define VERY_POSITIVE   (1e10)
#define VERY_NEGATIVE   (-1e10)

#define isforname(c) (isalnum(c) || (c) == '_' || (c) == '-' || (c) == '#')

#if defined(WIN32) && !defined(HAVE_STDINT_H)
// Define some useful C99 integer types.
typedef UINT64 uint64_t;
typedef  INT64  int64_t;
typedef UINT32 uint32_t;
typedef  INT32  int32_t;
typedef USHORT uint16_t;
typedef  SHORT  int16_t;
typedef  UCHAR  uint8_t;
typedef   CHAR   int8_t;
#endif

#if defined(WIN32)
std::string Narrow(const wchar_t *s);
std::wstring Widen(const char *s);
std::string Narrow(const std::wstring &s);
std::wstring Widen(const std::string &s);
#endif

inline double Random(double vmax) {
    return (vmax*rand()) / RAND_MAX;
}

class Expr;
class ExprVector;
class ExprQuaternion;
class RgbaColor;

//================
// From the platform-specific code.
#if defined(WIN32)
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

FILE *ssfopen(const std::string &filename, const char *mode);
void ssremove(const std::string &filename);

#define MAX_RECENT 8
#define RECENT_OPEN     (0xf000)
#define RECENT_IMPORT   (0xf100)
extern std::string RecentFile[MAX_RECENT];
void RefreshRecentMenus(void);

#define SAVE_YES     (1)
#define SAVE_NO     (-1)
#define SAVE_CANCEL  (0)
int SaveFileYesNoCancel(void);
int LoadAutosaveYesNo(void);

#define AUTOSAVE_SUFFIX "~"

#if defined(HAVE_GTK)
    // Selection pattern format to be parsed by GTK3 glue code:
    //   "PNG File\t*.png\n"
    //   "JPEG File\t*.jpg\t*.jpeg\n"
    //   "All Files\t*"
#   define PAT1(desc,e1)    desc "\t*." e1 "\n"
#   define PAT2(desc,e1,e2) desc "\t*." e1 "\t*." e2 "\n"
#   define ENDPAT "All Files\t*"
#elif defined(__APPLE__)
    // Selection pattern format to be parsed by Cocoa glue code:
    //   "PNG File\tpng\n"
    //   "JPEG file\tjpg,jpeg\n"
    //   "All Files\t*"
#   define PAT1(desc,e1)    desc "\t" e1 "\n"
#   define PAT2(desc,e1,e2) desc "\t" e1 "," e2 "\n"
#   define ENDPAT "All Files\t*"
#else
    // Selection pattern format for Win32's OPENFILENAME.lpstrFilter:
    //   "PNG File (*.png)\0*.png\0"
    //   "JPEG File (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0"
    //   "All Files (*)\0*\0\0"
#   define PAT1(desc,e1)    desc " (*." e1 ")\0*." e1 "\0"
#   define PAT2(desc,e1,e2) desc " (*." e1 ";*." e2 ")\0*." e1 ";*." e2 "\0"
#   define ENDPAT "All Files (*)\0*\0\0"
#endif

// SolveSpace native file format
#define SLVS_PATTERN PAT1("SolveSpace Models", "slvs") ENDPAT
#define SLVS_EXT "slvs"
// PNG format bitmap
#define PNG_PATTERN PAT1("PNG", "png") ENDPAT
#define PNG_EXT "png"
// Triangle mesh
#define MESH_PATTERN \
    PAT1("STL Mesh", "stl") \
    PAT1("Wavefront OBJ Mesh", "obj") \
    PAT1("Three.js-compatible JavaScript Mesh", "js") \
    ENDPAT
#define MESH_EXT "stl"
// NURBS surfaces
#define SRF_PATTERN PAT2("STEP File", "step", "stp") ENDPAT
#define SRF_EXT "step"
// 2d vector (lines and curves) format
#define VEC_PATTERN \
    PAT1("PDF File", "pdf") \
    PAT2("Encapsulated PostScript", "eps", "ps") \
    PAT1("Scalable Vector Graphics", "svg") \
    PAT2("STEP File", "step", "stp") \
    PAT1("DXF File", "dxf") \
    PAT2("HPGL File", "plt", "hpgl") \
    PAT1("G Code", "txt") \
    ENDPAT
#define VEC_EXT "pdf"
// 3d vector (wireframe lines and curves) format
#define V3D_PATTERN \
    PAT2("STEP File", "step", "stp") \
    PAT1("DXF File", "dxf") \
    ENDPAT
#define V3D_EXT "step"
// Comma-separated value, like a spreadsheet would use
#define CSV_PATTERN PAT1("CSV File", "csv") ENDPAT
#define CSV_EXT "csv"
bool GetSaveFile(std::string &filename, const char *defExtension, const char *selPattern);
bool GetOpenFile(std::string &filename, const char *defExtension, const char *selPattern);
void LoadAllFontFiles(void);

void OpenWebsite(const char *url);

void CheckMenuById(int id, bool checked);
void RadioMenuById(int id, bool selected);
void EnableMenuById(int id, bool enabled);

void ShowGraphicsEditControl(int x, int y, const std::string &str);
void HideGraphicsEditControl(void);
bool GraphicsEditControlIsVisible(void);
void ShowTextEditControl(int x, int y, const std::string &str);
void HideTextEditControl(void);
bool TextEditControlIsVisible(void);
void MoveTextScrollbarTo(int pos, int maxPos, int page);

#define CONTEXT_SUBMENU     (-1)
#define CONTEXT_SEPARATOR   (-2)
void AddContextMenuItem(const char *legend, int id);
void CreateContextSubmenu(void);
int ShowContextMenu(void);

void ToggleMenuBar(void);
bool MenuBarIsVisible(void);
void ShowTextWindow(bool visible);
void InvalidateText(void);
void InvalidateGraphics(void);
void PaintGraphics(void);
void ToggleFullScreen(void);
bool FullScreenIsActive(void);
void GetGraphicsWindowSize(int *w, int *h);
void GetTextWindowSize(int *w, int *h);
int64_t GetMilliseconds(void);
int64_t GetUnixTime(void);

void dbp(const char *str, ...);
#define DBPTRI(tri) \
    dbp("tri: (%.3f %.3f %.3f) (%.3f %.3f %.3f) (%.3f %.3f %.3f)", \
        CO((tri).a), CO((tri).b), CO((tri).c))

void SetCurrentFilename(const std::string &filename);
void SetMousePointerToHand(bool yes);
void DoMessageBox(const char *str, int rows, int cols, bool error);
void SetTimerFor(int milliseconds);
void SetAutosaveTimerFor(int minutes);
void ScheduleLater();
void ExitNow(void);

void CnfFreezeInt(uint32_t val, const std::string &name);
void CnfFreezeFloat(float val, const std::string &name);
void CnfFreezeString(const std::string &val, const std::string &name);
std::string CnfThawString(const std::string &val, const std::string &name);
uint32_t CnfThawInt(uint32_t val, const std::string &name);
float CnfThawFloat(float val, const std::string &name);

void *AllocTemporary(size_t n);
void FreeTemporary(void *p);
void FreeAllTemporary(void);
void *MemAlloc(size_t n);
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
void ssglLineWidth(GLfloat width);
void ssglVertex3v(Vector u);
void ssglAxisAlignedQuad(double l, double r, double t, double b, bool lone = true);
void ssglAxisAlignedLineLoop(double l, double r, double t, double b);
#define DEFAULT_TEXT_HEIGHT (11.5)
#ifdef WIN32
#   define SSGL_CALLBACK __stdcall
#else
#   define SSGL_CALLBACK
#endif
extern "C" { typedef void SSGL_CALLBACK ssglCallbackFptr(void); }
void ssglTesselatePolygon(GLUtesselator *gt, SPolygon *p);
void ssglFillPolygon(SPolygon *p);
void ssglFillMesh(bool useSpecColor, RgbaColor color,
    SMesh *m, uint32_t h, uint32_t s1, uint32_t s2);
void ssglDebugPolygon(SPolygon *p);
void ssglDrawEdges(SEdgeList *l, bool endpointsToo);
void ssglDebugMesh(SMesh *m);
void ssglMarkPolygonNormal(SPolygon *p);
typedef void ssglLineFn(void *data, Vector a, Vector b);
void ssglWriteText(const char *str, double h, Vector t, Vector u, Vector v,
    ssglLineFn *fn, void *fndata);
void ssglWriteTextRefCenter(const char *str, double h, Vector t, Vector u, Vector v,
    ssglLineFn *fn, void *fndata);
double ssglStrWidth(const char *str, double h);
double ssglStrHeight(double h);
void ssglLockColorTo(RgbaColor rgb);
void ssglFatLine(Vector a, Vector b, double width);
void ssglUnlockColor(void);
void ssglColorRGB(RgbaColor rgb);
void ssglColorRGBa(RgbaColor rgb, double a);
void ssglDepthRangeOffset(int units);
void ssglDepthRangeLockToFront(bool yes);
void ssglDrawPixelsWithTexture(uint8_t *data, int w, int h);
void ssglInitializeBitmapFont();
void ssglBitmapText(const char *str, Vector p);
void ssglBitmapCharQuad(char32_t chr, double x, double y);
int ssglBitmapCharWidth(char32_t chr);
#define TEXTURE_BACKGROUND_IMG  10
#define TEXTURE_DRAW_PIXELS     20
#define TEXTURE_COLOR_PICKER_2D 30
#define TEXTURE_COLOR_PICKER_1D 40
#define TEXTURE_BITMAP_FONT     50


#define arraylen(x) (sizeof((x))/sizeof((x)[0]))
#define PI (3.1415926535897931)
void MakeMatrix(double *mat, double a11, double a12, double a13, double a14,
                             double a21, double a22, double a23, double a24,
                             double a31, double a32, double a33, double a34,
                             double a41, double a42, double a43, double a44);
bool MakeAcceleratorLabel(int accel, char *out);
const char *ReadUTF8(const char *str, char32_t *chr);
bool StringAllPrintable(const char *str);
bool FilenameHasExtension(const std::string &str, const char *ext);
void Message(const char *str, ...);
void Error(const char *str, ...);
void CnfFreezeBool(bool v, const std::string &name);
void CnfFreezeColor(RgbaColor v, const std::string &name);
bool CnfThawBool(bool v, const std::string &name);
RgbaColor CnfThawColor(RgbaColor v, const std::string &name);

class System {
public:
    enum { MAX_UNKNOWNS = 1024 };

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
        SOLVED_OKAY          = 0,
        DIDNT_CONVERGE       = 10,
        SINGULAR_JACOBIAN    = 11,
        TOO_MANY_UNKNOWNS    = 20
    };
    int Solve(Group *g, int *dof, List<hConstraint> *bad,
                bool andFindBad, bool andFindFree);

    void Clear(void);
};

class TtfFont {
public:
    typedef struct {
        bool        onCurve;
        bool        lastInContour;
        int16_t     x;
        int16_t     y;
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

    std::string           fontFile;
    std::string           name;
    bool                  loaded;

    // The font itself, plus the mapping from ASCII codes to glyphs
    std::vector<uint16_t> charMap;
    std::vector<Glyph>    glyph;

    int                   maxPoints;
    int                   scale;

    // The filehandle, while loading
    FILE   *fh;
    // Some state while rendering a character to curves
    enum {
        NOTHING   = 0,
        ON_CURVE  = 1,
        OFF_CURVE = 2
    };
    int                   lastWas;
    IntPoint              lastOnCurve;
    IntPoint              lastOffCurve;

    // And the state that the caller must specify, determines where we
    // render to and how
    SBezierList *beziers;
    Vector      origin, u, v;

    int Getc(void);
    uint8_t GetBYTE(void);
    uint16_t GetUSHORT(void);
    uint32_t GetULONG(void);

    void LoadGlyph(int index);
    bool LoadFontFromFile(bool nameOnly);
    std::string FontFileBaseName(void);

    void Flush(void);
    void Handle(int *dx, int x, int y, bool onCurve);
    void PlotCharacter(int *dx, char32_t c, double spacing);
    void PlotString(const char *str, double spacing,
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

    void PlotString(const std::string &font, const char *str, double spacing,
                    SBezierList *sbl, Vector origin, Vector u, Vector v);
};

class StepFileWriter {
public:
    void ExportSurfacesTo(const std::string &filename);
    void WriteHeader(void);
	void WriteProductHeader(void);
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

    // This quells the Clang++ warning "'VectorFileWriter' has virtual
    // functions but non-virtual destructor"
    virtual ~VectorFileWriter() {}

    static double MmToPts(double mm);

    static VectorFileWriter *ForFile(const std::string &filename);

    void Output(SBezierLoopSetSet *sblss, SMesh *sm);

    void BezierAsPwl(SBezier *sb);
    void BezierAsNonrationalCubic(SBezier *sb, int depth=0);

    virtual void StartPath( RgbaColor strokeRgb, double lineWidth,
                            bool filled, RgbaColor fillRgb) = 0;
    virtual void FinishPath(RgbaColor strokeRgb, double lineWidth,
                            bool filled, RgbaColor fillRgb) = 0;
    virtual void Bezier(SBezier *sb) = 0;
    virtual void Triangle(STriangle *tr) = 0;
    virtual void StartFile(void) = 0;
    virtual void FinishAndCloseFile(void) = 0;
    virtual bool HasCanvasSize(void) = 0;

    virtual void Dummy(void);
};
class DxfFileWriter : public VectorFileWriter {
public:
    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb);
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb);
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

    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb);
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb);
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

    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb);
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb);
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

    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb);
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb);
    void Triangle(STriangle *tr);
    void Bezier(SBezier *sb);
    void StartFile(void);
    void FinishAndCloseFile(void);
    bool HasCanvasSize(void) { return true; }
};
class HpglFileWriter : public VectorFileWriter {
public:
    static double MmToHpglUnits(double mm);
    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb);
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb);
    void Triangle(STriangle *tr);
    void Bezier(SBezier *sb);
    void StartFile(void);
    void FinishAndCloseFile(void);
    bool HasCanvasSize(void) { return false; }
};
class Step2dFileWriter : public VectorFileWriter {
    StepFileWriter sfw;
    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb);
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb);
    void Triangle(STriangle *tr);
    void Bezier(SBezier *sb);
    void StartFile(void);
    void FinishAndCloseFile(void);
    bool HasCanvasSize(void) { return false; }
};
class GCodeFileWriter : public VectorFileWriter {
public:
    SEdgeList sel;
    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb);
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb);
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

    void Clear(void);
};
#undef ENTITY
#undef CONSTRAINT

class SolveSpaceUI {
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

        void Clear(void) {
            group.Clear();
            request.Clear();
            constraint.Clear();
            param.Clear();
            style.Clear();
        }
    } UndoState;
    enum { MAX_UNDO = 16 };
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
    enum { MODEL_COLORS = 8 };
    RgbaColor modelColor[MODEL_COLORS];
    Vector   lightDir[2];
    double   lightIntensity[2];
    double   ambientIntensity;
    double   chordTol;
    int      maxSegments;
    double   cameraTangent;
    float    gridSpacing;
    float    exportScale;
    float    exportOffset;
    bool     fixExportColors;
    bool     drawBackFaces;
    bool     checkClosedContour;
    bool     showToolbar;
    RgbaColor backgroundColor;
    bool     exportShadedTriangles;
    bool     exportPwlCurves;
    bool     exportCanvasSizeAuto;
    struct {
        float   left;
        float   right;
        float   bottom;
        float   top;
    }        exportMargin;
    struct {
        float   width;
        float   height;
        float   dx;
        float   dy;
    }        exportCanvas;
    struct {
        float   depth;
        int     passes;
        float   feed;
        float   plungeFeed;
    }        gCode;

    typedef enum {
        UNIT_MM = 0,
        UNIT_INCHES
    } Unit;
    Unit     viewUnits;
    int      afterDecimalMm;
    int      afterDecimalInch;
    int      autosaveInterval; // in minutes

    std::string MmToString(double v);
    double ExprToMm(Expr *e);
    double StringToMm(const std::string &s);
    const char *UnitName(void);
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
    void Init(void);
    bool OpenFile(const std::string &filename);
    void Exit(void);

    // File load/save routines, including the additional files that get
    // loaded when we have import groups.
    FILE        *fh;
    void AfterNewFile(void);
    static void RemoveFromRecentList(const std::string &filename);
    static void AddToRecentList(const std::string &filename);
    std::string saveFile;
    bool        fileLoadError;
    bool        unsaved;
    typedef struct {
        char        type;
        const char *desc;
        char        fmt;
        void       *ptr;
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
	bool Autosave();
    void RemoveAutosave();
    bool GetFilenameAndSave(bool saveAs);
    bool OkayToStartNewFile(void);
    hGroup CreateDefaultDrawingGroup(void);
    void UpdateWindowTitle(void);
    void ClearExisting(void);
    void NewFile(void);
    bool SaveToFile(const std::string &filename);
    bool LoadAutosaveFor(const std::string &filename);
    bool LoadFromFile(const std::string &filename);
    bool LoadEntitiesFromFile(const std::string &filename, EntityList *le,
                              SMesh *m, SShell *sh);
    void ReloadAllImported(void);
    // And the various export options
    void ExportAsPngTo(const std::string &filename);
    void ExportMeshTo(const std::string &filename);
    void ExportMeshAsStlTo(FILE *f, SMesh *sm);
    void ExportMeshAsObjTo(FILE *f, SMesh *sm);
    void ExportMeshAsThreeJsTo(FILE *f, const std::string &filename, SMesh *sm, SEdgeList *sel);
    void ExportViewOrWireframeTo(const std::string &filename, bool wireframe);
    void ExportSectionTo(const std::string &filename);
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
        uint8_t     *fromFile;
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
        bool    scheduled;
        bool    showTW;
        bool    generateAll;
    } later;
    void ScheduleShowTW();
    void ScheduleGenerateAll();
    void DoLater(void);

    static void MenuHelp(int id);

    void Clear(void);
};

extern SolveSpaceUI SS;
extern Sketch SK;

};

#ifndef __OBJC__
using namespace SolveSpace;
#endif

#endif
