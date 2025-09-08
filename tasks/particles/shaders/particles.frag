#version 450

#extension GL_GOOGLE_include_directive : require
#include "UniformParams.h"

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vUV;
layout(location = 2) in float vAlpha;

layout(set = 0, binding = 0) uniform sampler2D particleTex;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(particleTex, vUV);

    // фінальний колір з урахуванням прозорості
    //outColor = vec4(texColor.rgb, texColor.a * vAlpha);
    vec3 color = vec3(0.0, 0.0, 1.0);
    outColor = vec4(vColor.rgb, vAlpha);

    // відкидаємо прозорі фрагменти
    if (outColor.a < 0.01)
        discard;
}
