//-----------------------------------------------------------------------------
// Indexed Mesh rendering shader
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
uniform vec4 color;
uniform sampler2D texture_;

varying vec2 fragTex;

void main() {
    vec4 texColor = texture2D(texture_, fragTex);
    if(texColor.a == 0.0) discard;
    gl_FragColor = texColor * color;
}
