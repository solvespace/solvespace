//-----------------------------------------------------------------------------
// Point rendering shader
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
const float feather = 0.5;

uniform vec4 color;
uniform float pixel;
uniform float width;

void main() {
    // Rectangular points
    gl_FragColor = color;
}
