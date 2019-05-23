//-----------------------------------------------------------------------------
// Mesh rendering shader
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
uniform vec4 color;
uniform sampler2D texture_;

void main() {
    if(texture2D(texture_, gl_FragCoord.xy / 32.0).TEX_ALPHA < 0.5) discard;
    gl_FragColor = color;
}
