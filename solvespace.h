
#ifndef __SOLVESPACE_H
#define __SOLVESPACE_H

// Debugging functions
#define oops() do { dbp("oops at line %d, file %s", __LINE__, __FILE__); \
                                                        exit(-1); } while(0)
#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif

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

// From the platform-specific code.
int SaveFileYesNoCancel(void);
BOOL GetSaveFile(char *file, char *defExtension, char *selPattern);
BOOL GetOpenFile(char *file, char *defExtension, char *selPattern);

void CheckMenuById(int id, BOOL checked);
void EnableMenuById(int id, BOOL checked);

void ShowGraphicsEditControl(int x, int y, char *s);
void HideGraphicsEditControl(void);
BOOL GraphicsEditControlIsVisible(void);

void InvalidateGraphics(void);
void InvalidateText(void);
SDWORD GetMilliseconds(void);
void PaintGraphics(void);

void dbp(char *str, ...);
void Error(char *str, ...);

void *AllocTemporary(int n);
void FreeAllTemporary(void);
void *MemRealloc(void *p, int n);
void *MemAlloc(int n);
void MemFree(void *p);


#include "dsc.h"
#include "polygon.h"
#include "sketch.h"
#include "ui.h"
#include "expr.h"


// Utility functions that are provided in the platform-independent code.
void glxVertex3v(Vector u);
void glxFillPolygon(SPolygon *p);
void glxWriteText(char *str);
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
    // has been assigned to; these are exceptions.
    static const int ASSUMED          = 10000;
    static const int SUBSTITUTED      = 10001;

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
        double      X[MAX_UNKNOWNS];
        struct {
            Expr        *sym[MAX_UNKNOWNS];
            double       num[MAX_UNKNOWNS];
        }           B;
    } mat;

    bool Tol(double v);
    void GaussJordan(void);
    bool SolveLinearSystem(void);

    void WriteJacobian(int eqTag, int paramTag);
    void EvalJacobian(void);

    bool NewtonSolve(int tag);
    bool Solve(void);
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

    hGroup      activeGroup;

    FILE        *fh;

    void GenerateAll(void);
    void ForceReferences(void);

    void Init(char *cmdLine);

    bool SolveGroup(hGroup hg);
    bool SolveWorker(int order);
    void Solve(void);

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
    struct {
        Group        g;
        Request      r;
        Entity       e;
        Param        p;
        Constraint   c;
    } sv;
    static void MenuFile(int id);
    void NewFile(void);
    bool SaveToFile(char *filename);
    bool LoadFromFile(char *filename);

    // The system to be solved.
    System  sys;
};

extern SolveSpace SS;

#endif
