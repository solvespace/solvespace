//-----------------------------------------------------------------------------
// Mesh rendering shader
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
attribute vec3 pos;

uniform mat4 modelview;
uniform mat4 projection;

void main() {
    gl_Position = projection * modelview * vec4(pos, 1.0);
}
