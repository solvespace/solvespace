//-----------------------------------------------------------------------------
// Outline rendering shader
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
const int EMPHASIZED_AND_CONTOUR        = 0;
const int EMPHASIZED_WITHOUT_CONTOUR    = 1;
const int CONTOUR_ONLY                  = 2;

const float feather = 0.5;

attribute vec3 pos;
attribute vec4 loc;
attribute vec3 tgt;
attribute vec3 nol;
attribute vec3 nor;

uniform mat4 modelview;
uniform mat4 projection;
uniform float width;
uniform float pixel;
uniform int mode;

varying vec3 fragLoc;

void main() {
    // get camera direction from modelview matrix
    vec3 dir = vec3(modelview[0].z, modelview[1].z, modelview[2].z);

    // perform outline visibility test
    float ldot = dot(nol, dir);
    float rdot = dot(nor, dir);

    bool isOutline = (ldot > -1e-6) == (rdot < 1e-6) ||
                     (rdot > -1e-6) == (ldot < 1e-6);
    bool isTagged = loc.w > 0.5;

    float visible = float((mode == CONTOUR_ONLY && isOutline) ||
                          (mode == EMPHASIZED_AND_CONTOUR && (isOutline || isTagged)) ||
                          (mode == EMPHASIZED_WITHOUT_CONTOUR && isTagged && !isOutline));

    // calculate line contour extension basis for constant width and caps
    vec3 norm = normalize(cross(tgt, dir));
    norm = normalize(norm - dir * dot(dir, norm));
    vec3 perp = normalize(cross(dir, norm));

    // calculate line extension width considering antialiasing
    float ext = (width + feather * pixel) * visible;

    // extend line contour
    vec3 vertex = pos;
    vertex += ext * loc.x * normalize(perp);
    vertex += ext * loc.y * normalize(norm);

    // write fragment location for calculating caps and antialiasing
    fragLoc = vec3(loc);

    // transform resulting vertex with modelview and projection matrices
    gl_Position = projection * modelview * vec4(vertex, 1.0);
}
