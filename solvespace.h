
#ifndef __SOLVESPACE_H
#define __SOLVESPACE_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dsc.h"
#include "ui.h"
#include "sketch.h"

// Debugging functions
#define oops() do { dbp("oops at line %d, file %s", __LINE__, __FILE__); \
                                                        exit(-1); } while(0)
void dbp(char *str, ...);

#define arraylen(x) (sizeof((x))/sizeof((x)[0]))

typedef struct {
    TextWindow                  TW;
    GraphicsWindow              GW;

    IdList<Request,hRequest>    req;
    IdList<Entity,hEntity>      entity;
    IdList<Point,hPoint>        point;
    IdList<Param,hParam>        param;

    void Init(void);
} SolveSpace;

extern SolveSpace SS;

#endif
