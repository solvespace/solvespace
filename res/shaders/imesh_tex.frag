//-----------------------------------------------------------------------------
// SolveSpace Indexed Mesh rendering shader
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
#version 120

uniform vec4 color;
uniform sampler2D texture;

varying vec2 fragTex;

void main() {
    gl_FragColor = texture2D(texture, fragTex) * color;
}
