//-----------------------------------------------------------------------------
// Point rendering shader
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
const float feather = 0.5;

attribute vec3 pos;
attribute vec2 loc;

uniform mat4 modelview;
uniform mat4 projection;
uniform float width;
uniform float pixel;

void main() {
    // get camera vectors from modelview matrix
    vec3 u = vec3(modelview[0].x, modelview[1].x, modelview[2].x);
    vec3 v = vec3(modelview[0].y, modelview[1].y, modelview[2].y);

    // calculate point contour extension basis for constant width and caps

    // calculate point extension width considering antialiasing
    float ext = width + feather * pixel;

    // extend point contour
    vec3 vertex = pos;
    vertex += ext * loc.x * normalize(u);
    vertex += ext * loc.y * normalize(v);

    // transform resulting vertex with modelview and projection matrices
    gl_Position = projection * modelview * vec4(vertex, 1.0);
}
