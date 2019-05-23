//-----------------------------------------------------------------------------
// Indexed Mesh rendering shader
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
uniform vec4 color;
uniform sampler2D texture_;

varying vec2 fragTex;

void main() {
    gl_FragColor = vec4(color.rgb, color.a * texture2D(texture_, fragTex).TEX_ALPHA);
}
