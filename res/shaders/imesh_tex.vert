//-----------------------------------------------------------------------------
// Indexed Mesh rendering shader
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
attribute vec3 pos;
attribute vec2 tex;

uniform mat4 modelview;
uniform mat4 projection;

varying vec2 fragTex;

void main() {
    fragTex = tex;
    gl_Position =  projection * modelview * vec4(pos, 1.0);
}
