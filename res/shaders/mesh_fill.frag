//-----------------------------------------------------------------------------
// SolveSpace Mesh rendering shader
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
#version 120

uniform vec4 color;
uniform sampler2D texture;

void main() {
    if(texture2D(texture, gl_FragCoord.xy / 32.0f).a < 0.5) discard;
    gl_FragColor = color;
}
