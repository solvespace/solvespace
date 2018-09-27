//
// Created by benjamin on 9/27/18.
//

#ifndef SOLVESPACE_LIGHTING_H
#define SOLVESPACE_LIGHTING_H

#include "dsc.h"

namespace SolveSpace {

// A description of scene lighting.
class Lighting {
public:
    RgbaColor   backgroundColor;
    double      ambientIntensity;
    double      lightIntensity[2];
    Vector      lightDirection[2];
};

}

#endif //SOLVESPACE_LIGHTING_H
