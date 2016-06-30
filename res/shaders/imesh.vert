//-----------------------------------------------------------------------------
// SolveSpace Indexed Mesh rendering shader
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
#version 120

attribute vec3 pos;

uniform mat4 modelview;
uniform mat4 projection;

void main() {
    gl_Position =  projection * modelview * vec4(pos, 1.0);
}
