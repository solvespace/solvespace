//-----------------------------------------------------------------------------
// Mesh rendering shader
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
uniform vec3 lightDir0;
uniform vec3 lightDir1;
uniform float lightInt0;
uniform float lightInt1;
uniform float ambient;

varying vec3 fragNormal;
varying vec4 fragColor;

void main() {
    vec3 result = fragColor.xyz * ambient;
    vec3 normal = normalize(fragNormal);

    float light0 = clamp(dot(lightDir0, normal), 0.0, 1.0) * lightInt0 * (1.0 - ambient);
    result += fragColor.rgb * light0;

    float light1 = clamp(dot(lightDir1, normal), 0.0, 1.0) * lightInt1 * (1.0 - ambient);
    result += fragColor.rgb * light1;

    gl_FragColor = vec4(result, fragColor.a);
}
