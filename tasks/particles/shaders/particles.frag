#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in float vAlpha;

layout(set = 0, binding = 0) uniform sampler2D particleTex;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(particleTex, vUV);

    // фінальний колір з урахуванням прозорості
    outColor = vec4(texColor.rgb, texColor.a * vAlpha);

    // відкидаємо прозорі фрагменти
    if (outColor.a < 0.01)
        discard;
}
