
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
    while(v >= n) v -= n;
    while(v < 0) v += n;
    return v;
}

#define SWAP(T, a, b) do { T temp = (a); (a) = (b); (b) = temp; } while(0)
#define ZERO(v) memset((v), 0, sizeof(*(v)))
#define CO(v) (v).x, (v).y, (v).z

#define LENGTH_EPS  (0.0000001)

#define isforname(c) (isalnum(c) || (c) == '_' || (c) == '-' || (c) == '#')

typedef signed long SDWORD;

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


// From the platform-specific code.
#define MAX_RECENT 8
#define RECENT_OPEN     (0xf000)
#define RECENT_IMPORT   (0xf100)
extern char RecentFile[MAX_RECENT][MAX_PATH];
void RefreshRecentMenus(void);

int SaveFileYesNoCancel(void);
#define SLVS_PATTERN "SolveSpace Models (*.slvs)\0*.slvs\0All Files (*)\0*\0\0"
#define SLVS_EXT "slvs"
BOOL GetSaveFile(char *file, char *defExtension, char *selPattern);
BOOL GetOpenFile(char *file, char *defExtension, char *selPattern);

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
SDWORD GetMilliseconds(void);

void dbp(char *str, ...);
#define DBPTRI(tri) \
    dbp("tri: (%.3f %.3f %.3f) (%.3f %.3f %.3f) (%.3f %.3f %.3f)", \
        CO((tri).a), CO((tri).b), CO((tri).c))

void Error(char *str, ...);

void *AllocTemporary(int n);
void FreeAllTemporary(void);
void *MemRealloc(void *p, int n);
void *MemAlloc(int n);
void MemFree(void *p);
void vl(void); // debug function to validate


#include "dsc.h"
#include "polygon.h"

class Entity;
class hEntity;
typedef IdList<Entity,hEntity> EntityList;

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
void glxDebugEdgeList(SEdgeList *l);
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


#define arraylen(x) (sizeof((x))/sizeof((x)[0]))
#define PI (3.1415926535897931)
void MakeMatrix(double *mat, double a11, double a12, double a13, double a14,
                             double a21, double a22, double a23, double a24,
                             double a31, double a32, double a33, double a34,
                             double a41, double a42, double a43, double a44);

class System {
public:
#define MAX_UNKNOWNS    200

    IdList<Entity,hEntity>          entity;
    IdList<Param,hParam>            param;
    IdList<Equation,hEquation>      eq;

    // In general, the tag indicates the subsys that a variable/equation
    // has been assigned to; these are exceptions for variables:
    static const int VAR_ASSUMED          = 10000;
    static const int VAR_SUBSTITUTED      = 10001;
    // and for equations:
    static const int EQ_SUBSTITUTED       = 20000;

    // The system Jacobian matrix
    struct {
        // The corresponding equation for each row
        hEquation   eq[MAX_UNKNOWNS];

        // The corresponding parameter for each column
        hParam      param[MAX_UNKNOWNS];

        bool        bound[MAX_UNKNOWNS];

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

    bool Tol(double v);
    int GaussJordan(void);
    static bool SolveLinearSystem(double X[], double A[][MAX_UNKNOWNS],
                                  double B[], int N);
    bool SolveLeastSquares(void);

    void WriteJacobian(int eqTag, int paramTag);
    void EvalJacobian(void);

    void WriteEquationsExceptFor(hConstraint hc, hGroup hg);
    void FindWhichToRemoveToFixJacobian(Group *g);
    void SolveBySubstitution(void);

    static bool IsDragged(hParam p);

    bool NewtonSolve(int tag);
    void Solve(Group *g);
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

    FILE        *fh;

    void Init(char *cmdLine);
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
    void NewFile(void);
    bool SaveToFile(char *filename);
    bool LoadFromFile(char *filename);
    bool LoadEntitiesFromFile(char *filename, EntityList *le, SMesh *m);
    void ReloadAllImported(void);

    void MarkGroupDirty(hGroup hg);
    void MarkGroupDirtyByEntity(hEntity he);

    struct {
        int     requests;
        int     groups;
        int     constraints;
    } deleted;
    bool GroupExists(hGroup hg);
    bool PruneOrphans(void);
    bool EntityExists(hEntity he);
    bool GroupsInOrder(hGroup before, hGroup after);
    bool PruneGroups(hGroup hg);
    bool PruneRequests(hGroup hg);
    bool PruneConstraints(hGroup hg);

    void GenerateAll(void);
    void GenerateAll(int first, int last);
    void SolveGroup(hGroup hg);
    void ForceReferences(void);

    // The system to be solved.
    System  sys;

    struct {
        bool    showTW;
        bool    generateAll;
    } later;
    void DoLater(void);
};

extern SolveSpace SS;

#endif
