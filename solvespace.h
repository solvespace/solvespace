
#ifndef __SOLVESPACE_H
#define __SOLVESPACE_H

#include <stdlib.h>
#include <string.h>
#include "dsc.h"
#include "ui.h"

// Debugging functions
#define oops() exit(-1)
void dbp(char *str, ...);

typedef struct {
    TextWindow      TW;
    GraphicsWindow  GW;

    void Init(void);
} SolveSpace;

extern SolveSpace SS;

#endif
