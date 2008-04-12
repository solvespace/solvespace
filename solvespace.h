
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

void dbp(char *str, ...);
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <windows.h> // required for GL stuff
#include <gl/gl.h>
#include <gl/glu.h>

class Expr;
#include "dsc.h"
#include "sketch.h"
#include "ui.h"
#include "expr.h"

// From the platform-specific code.
void InvalidateGraphics(void);
void InvalidateText(void);

// Utility functions that are provided in the platform-independent code.
void glxVertex3v(Vector u);
void glxWriteText(char *str);
void glxTranslatev(Vector u);
void glxOntoCsys(Vector u, Vector v);


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

    IdList<Group,hGroup>        group;
    IdList<Request,hRequest>    request;
    IdList<Entity,hEntity>      entity;
    IdList<Point,hPoint>        point;
    IdList<Param,hParam>        param;

    hGroup                      activeGroup;

    void Init(void);
    void Solve(void);
};

extern SolveSpace SS;

#endif
