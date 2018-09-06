//-----------------------------------------------------------------------------
// Edge rendering shader
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
const float feather = 0.5;

uniform vec4 color;
uniform float pixel;
uniform float width;
uniform float patternLen;
uniform float patternScale;
uniform sampler2D pattern;

varying vec3 fragLoc;

void main() {
    // lookup distance texture
    vec4 v = texture2D(pattern, vec2(fragLoc.z / patternScale, 0.0));

    // decode distance value
    float val = dot(v, vec4(1.0, 1.0 / 255.0, 1.0 / 65025.0, 1.0 / 160581375.0));

    // calculate cap
    float dist = length(vec2(val * patternScale / (patternLen * width) + abs(fragLoc.x), fragLoc.y));

    // perform antialiasing
    float k = smoothstep(1.0 - 2.0 * feather * pixel / (width + feather * pixel), 1.0, abs(dist));

    // perform alpha-test
    if(k == 1.0) discard;

    // write resulting color
    gl_FragColor = vec4(color.rgb, color.a * (1.0 - k));
}
