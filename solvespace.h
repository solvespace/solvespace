
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

// From the platform-specific code.
int SaveFileYesNoCancel(void);
BOOL GetSaveFile(char *file, char *defExtension, char *selPattern);
BOOL GetOpenFile(char *file, char *defExtension, char *selPattern);

void CheckMenuById(int id, BOOL checked);
void EnableMenuById(int id, BOOL checked);

void InvalidateGraphics(void);
void InvalidateText(void);
SDWORD GetMilliseconds(void);
void PaintGraphics(void);

void dbp(char *str, ...);
void Error(char *str, ...);

Expr *AllocExpr(void);
void FreeAllExprs(void);
void *MemRealloc(void *p, int n);
void *MemAlloc(int n);
void MemFree(void *p);


#include "dsc.h"
#include "sketch.h"
#include "ui.h"
#include "expr.h"


// Utility functions that are provided in the platform-independent code.
void glxVertex3v(Vector u);
void glxWriteText(char *str);
void glxTranslatev(Vector u);
void glxOntoCsys(Vector u, Vector v);
void glxLockColorTo(double r, double g, double b);
void glxUnlockColor(void);
void glxColor(double r, double g, double b);


#define arraylen(x) (sizeof((x))/sizeof((x)[0]))
#define PI (3.1415926535897931)
void MakeMatrix(double *mat, double a11, double a12, double a13, double a14,
                             double a21, double a22, double a23, double a24,
                             double a31, double a32, double a33, double a34,
                             double a41, double a42, double a43, double a44);


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

    hGroup      activeGroup;

    FILE        *fh;

    void GenerateAll(void);
    void ForceReferences(void);

    void Init(void);
    void Solve(void);

    static void MenuFile(int id);
    bool SaveToFile(char *filename);
    bool LoadFromFile(char *filename);
};

extern SolveSpace SS;

#endif
