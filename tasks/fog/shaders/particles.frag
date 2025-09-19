#version 450

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vUV;
layout(location = 2) in float vAlpha;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(vColor.rgb, vAlpha);
    if (outColor.a < 0.01)
        discard;
}
