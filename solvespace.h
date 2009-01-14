
#ifndef __SOLVESPACE_H
#define __SOLVESPACE_H

// Debugging functions
#define oops() do { dbp("oops at line %d, file %s", __LINE__, __FILE__); \
                    if(1) *(char *)0 = 1; exit(-1); } while(0)
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

#define SWAP(T, a, b) do { T temp = (a); (a) = (b); (b) = temp; } while(0)
#define ZERO(v) memset((v), 0, sizeof(*(v)))
#define CO(v) (v).x, (v).y, (v).z

#define LENGTH_EPS  (1e-7)

#define isforname(c) (isalnum(c) || (c) == '_' || (c) == '-' || (c) == '#')

typedef signed long SDWORD;
typedef signed short SWORD;

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <windows.h> // required for GL stuff
#include <gl/gl.h>
#include <gl/glu.h>

class Expr;
class ExprVector;
class ExprQuaternion;


//================
// From the platform-specific code.
#define MAX_RECENT 8
#define RECENT_OPEN     (0xf000)
#define RECENT_IMPORT   (0xf100)
extern char RecentFile[MAX_RECENT][MAX_PATH];
void RefreshRecentMenus(void);

int SaveFileYesNoCancel(void);
#define SLVS_PATTERN "SolveSpace Models (*.slvs)\0*.slvs\0All Files (*)\0*\0\0"
#define SLVS_EXT "slvs"
#define PNG_PATTERN "PNG (*.png)\0*.png\0All Files (*)\0*\0\0"
#define PNG_EXT "png"
#define STL_PATTERN "STL Mesh (*.stl)\0*.stl\0All Files (*)\0*\0\0"
#define STL_EXT "stl"
#define DXF_PATTERN "DXF File (*.dxf)\0*.dxf\0All Files (*)\0*\0\0"
#define DXF_EXT "dxf"
#define CSV_PATTERN "CSV File (*.csv)\0*.csv\0All Files (*)\0*\0\0"
#define CSV_EXT "csv"
#define LICENSE_PATTERN \
    "License File (*.license)\0*.license\0All Files (*)\0*\0\0"
#define LICENSE_EXT "license"
BOOL GetSaveFile(char *file, char *defExtension, char *selPattern);
BOOL GetOpenFile(char *file, char *defExtension, char *selPattern);
void GetAbsoluteFilename(char *file);
void LoadAllFontFiles(void);

void OpenWebsite(char *url);

void CheckMenuById(int id, BOOL checked);
void EnableMenuById(int id, BOOL checked);

void ShowGraphicsEditControl(int x, int y, char *s);
void HideGraphicsEditControl(void);
BOOL GraphicsEditControlIsVisible(void);
void ShowTextEditControl(int hr, int c, char *s);
void HideTextEditControl(void);
BOOL TextEditControlIsVisible(void);

void ShowTextWindow(BOOL visible);
void InvalidateText(void);
void InvalidateGraphics(void);
void PaintGraphics(void);
void GetGraphicsWindowSize(int *w, int *h);
SDWORD GetMilliseconds(void);

void dbp(char *str, ...);
#define DBPTRI(tri) \
    dbp("tri: (%.3f %.3f %.3f) (%.3f %.3f %.3f) (%.3f %.3f %.3f)", \
        CO((tri).a), CO((tri).b), CO((tri).c))

void SetWindowTitle(char *str);
void Message(char *str, ...);
void Error(char *str, ...);
void SetTimerFor(int milliseconds);
void ExitNow(void);

void DrawWithBitmapFont(char *str);
void GetBitmapFontExtent(char *str, int *w, int *h);

void CnfFreezeString(char *str, char *name);
void CnfFreezeDWORD(DWORD v, char *name);
void CnfFreezeFloat(float v, char *name);
void CnfThawString(char *str, int maxLen, char *name);
DWORD CnfThawDWORD(DWORD v, char *name);
float CnfThawFloat(float v, char *name);

void *AllocTemporary(int n);
void FreeTemporary(void *p);
void FreeAllTemporary(void);
void *MemRealloc(void *p, int n);
void *MemAlloc(int n);
void MemFree(void *p);
void vl(void); // debug function to validate heaps

// End of platform-specific functions
//================


#include "dsc.h"
#include "polygon.h"

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
#define GLX_CALLBACK __stdcall
typedef void GLX_CALLBACK glxCallbackFptr(void);
void glxTesselatePolygon(GLUtesselator *gt, SPolygon *p);
void glxFillPolygon(SPolygon *p);
void glxFillMesh(int color, SMesh *m, DWORD h, DWORD s1, DWORD s2);
void glxDebugPolygon(SPolygon *p);
void glxDrawEdges(SEdgeList *l);
void glxDebugMesh(SMesh *m);
void glxMarkPolygonNormal(SPolygon *p);
void glxWriteText(char *str);
void glxWriteTextRefCenter(char *str);
double glxStrWidth(char *str);
double glxStrHeight(void);
void glxTranslatev(Vector u);
void glxOntoWorkplane(Vector u, Vector v);
void glxLockColorTo(double r, double g, double b);
void glxUnlockColor(void);
void glxColor3d(double r, double g, double b);
void glxColor4d(double r, double g, double b, double a);
void glxDepthRangeOffset(int units);
void glxDepthRangeLockToFront(bool yes);


#define arraylen(x) (sizeof((x))/sizeof((x)[0]))
#define PI (3.1415926535897931)
void MakeMatrix(double *mat, double a11, double a12, double a13, double a14,
                             double a21, double a22, double a23, double a24,
                             double a31, double a32, double a33, double a34,
                             double a41, double a42, double a43, double a44);
void MakePathRelative(char *base, char *path);
void MakePathAbsolute(char *base, char *path);

class System {
public:
#define MAX_UNKNOWNS    200

    EntityList                      entity;
    ParamList                       param;
    IdList<Equation,hEquation>      eq;

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
    static bool SolveLinearSystem(double X[], double A[][MAX_UNKNOWNS],
                                  double B[], int N);
    bool SolveLeastSquares(void);

    void WriteJacobian(int tag);
    void EvalJacobian(void);

    void WriteEquationsExceptFor(hConstraint hc, hGroup hg);
    void FindWhichToRemoveToFixJacobian(Group *g);
    void SolveBySubstitution(void);

    static bool IsDragged(hParam p);

    bool NewtonSolve(int tag);
    void Solve(Group *g, bool andFindFree);
};

class TtfFont {
public:
    typedef struct {
        bool        onCurve;
        bool        lastInContour;
        SWORD       x;
        SWORD       y;       
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
    hEntity     entity;
    Vector      origin, u, v;

    int Getc(void);
    int GetBYTE(void);
    int GetWORD(void);
    int GetDWORD(void);

    void LoadGlyph(int index);
    bool LoadFontFromFile(bool nameOnly);
    char *FontFileBaseName(void);
   
    void Flush(void);
    void Handle(int *dx, int x, int y, bool onCurve);
    void PlotCharacter(int *dx, int c, double spacing);
    void PlotString(char *str, double spacing,
                    hEntity he, Vector origin, Vector u, Vector v);

    Vector TransformIntPoint(int x, int y);
    void LineSegment(int x0, int y0, int x1, int y1);
    void Bezier(int x0, int y0, int x1, int y1, int x2, int y2);
    void BezierPwl(double ta, double tb, Vector p0, Vector p1, Vector p2);
    Vector BezierEval(double t, Vector p0, Vector p1, Vector p2);
};

class TtfFontList {
public:
    bool                loaded;
    List<TtfFont>       l;

    void LoadAll(void);

    void PlotString(char *font, char *str, double spacing,
                    hEntity he, Vector origin, Vector u, Vector v);
};

class VectorFileWriter {
public:
    FILE *f;
    
    static bool StringEndsIn(char *str, char *ending);
    static VectorFileWriter *ForFile(char *file);

    virtual void SetLineWidth(double mm) = 0;
    virtual void LineSegment(double x0, double y0, double x1, double y1) = 0;
    virtual void StartFile(void) = 0;
    virtual void FinishAndCloseFile(void) = 0;
};
class DxfFileWriter : public VectorFileWriter {
public:
    void SetLineWidth(double mm);
    void LineSegment(double x0, double y0, double x1, double y1);
    void StartFile(void);
    void FinishAndCloseFile(void);
};

class SolveSpace {
public:
    TextWindow                  TW;
    GraphicsWindow              GW;

    // These lists define the sketch, and are edited by the user.
    IdList<Group,hGroup>            group;
    IdList<Request,hRequest>        request;
    IdList<Constraint,hConstraint>  constraint;

    // These lists are generated automatically when we solve the sketch.
    IdList<Entity,hEntity>          entity;
    IdList<Param,hParam>            param;

    inline Constraint *GetConstraint(hConstraint h)
        { return constraint.FindById(h); }
    inline Request *GetRequest(hRequest h) { return request.FindById(h); }
    inline Entity  *GetEntity (hEntity  h) { return entity. FindById(h); }
    inline Param   *GetParam  (hParam   h) { return param.  FindById(h); }
    inline Group   *GetGroup  (hGroup   h) { return group.  FindById(h); }

    // The state for undo/redo
    typedef struct {
        IdList<Group,hGroup>            group;
        IdList<Request,hRequest>        request;
        IdList<Constraint,hConstraint>  constraint;
        IdList<Param,hParam>            param;
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
    double  chordTol;
    int     maxSegments;
    double  cameraTangent;
    DWORD   edgeColor;
    float   exportScale;
    float   exportOffset;
    int     drawBackFaces;
    int     showToolbar;

    int CircleSides(double r);
    typedef enum {
        UNIT_MM = 0,
        UNIT_INCHES,
    } Unit;
    Unit    viewUnits;
    char *MmToString(double v);
    double ExprToMm(Expr *e);
    double StringToMm(char *s);

    // The platform-dependent code calls this before entering the msg loop
    void Init(char *cmdLine);
    void CheckLicenseFromRegistry(void);
    void Exit(void);

    // File load/save routines, including the additional files that get
    // loaded when we have import groups.
    FILE        *fh;
    void AfterNewFile(void);
    static void RemoveFromRecentList(char *file);
    static void AddToRecentList(char *file);
    char saveFile[MAX_PATH];
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
    } sv;
    static void MenuFile(int id);
    bool GetFilenameAndSave(bool saveAs);
    bool OkayToStartNewFile(void);
    hGroup CreateDefaultDrawingGroup(void);
    void UpdateWindowTitle(void);
    void NewFile(void);
    bool SaveToFile(char *filename);
    bool LoadFromFile(char *filename);
    bool LoadEntitiesFromFile(char *filename, EntityList *le, SMesh *m);
    void ReloadAllImported(void);
    // And the various export options
    void ExportAsPngTo(char *file);
    void ExportMeshTo(char *file);
    void ExportViewTo(char *file);
    void ExportSectionTo(char *file);
    void ExportPolygon(SPolygon *sp, 
                       Vector u, Vector v, Vector n, Vector origin,
                       VectorFileWriter *out);

    static void MenuAnalyze(int id);
    struct {
        SContour    path;
        hEntity     point;
    } traced;

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

    // For the licensing
    class Crc {
public:
        static const DWORD POLY = 0xedb88320;
        DWORD shiftReg;
        
        void ProcessBit(int bit);
        void ProcessByte(BYTE b);
        void ProcessString(char *s);
    };
    Crc crc;
    struct {
        bool licensed;
        char line1[512];
        char line2[512];
        char users[512];
        DWORD key;
    } license;
    static void MenuHelp(int id);
    void CleanEol(char *s);
    void LoadLicenseFile(char *filename);
    bool LicenseValid(char *line1, char *line2, char *users, DWORD key);
};

extern SolveSpace SS;

#endif
