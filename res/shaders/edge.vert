//-----------------------------------------------------------------------------
// Edge rendering shader
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
const float feather = 0.5;

attribute vec3 pos;
attribute vec3 loc;
attribute vec3 tgt;

uniform mat4 modelview;
uniform mat4 projection;
uniform float width;
uniform float pixel;

varying vec3 fragLoc;

void main() {
    // get camera direction from modelview matrix
    vec3 dir = vec3(modelview[0].z, modelview[1].z, modelview[2].z);

    // calculate line contour extension basis for constant width and caps
    vec3 norm = normalize(cross(tgt, dir));
    norm = normalize(norm - dir * dot(dir, norm));
    vec3 perp = normalize(cross(dir, norm));

    // calculate line extension width considering antialiasing
    float ext = width + feather * pixel;

    // extend line contour
    vec3 vertex = pos;
    vertex += ext * loc.x * normalize(perp);
    vertex += ext * loc.y * normalize(norm);

    // write fragment location for calculating caps and antialiasing
    fragLoc = loc;

    // transform resulting vertex with modelview and projection matrices
    gl_Position = projection * modelview * vec4(vertex, 1.0);
}
