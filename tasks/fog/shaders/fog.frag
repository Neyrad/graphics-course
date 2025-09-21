#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 fragPos;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec4 lightSpacePos;
layout(location = 3) in vec4 coolColor;
layout(location = 4) in vec4 skyboxColor;

layout(binding = 3) uniform sampler2D shadowMap;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 lightPos;
    uint id;
} push;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightDir = normalize(push.lightPos.xyz - fragPos.xyz);

    vec4 baseColor = push.id == 0 ? coolColor : skyboxColor;
    
    vec3 projCoords = (lightSpacePos.xyz / lightSpacePos.w) * 0.5 + 0.5;

    const bool  outOfView = (projCoords.x < 0.0001f || projCoords.x > 0.9999f || projCoords.y < 0.0091f || projCoords.y > 0.9999f);
    const float shadow    = ((projCoords.z < textureLod(shadowMap, projCoords.xy, 0).x + 0.001f) || outOfView) ? 1.0f : 0.0f;

    const vec4 dark_violet = vec4(0.59f, 0.0f, 0.82f, 1.0f);
    const vec4 chartreuse  = vec4(0.5f, 1.0f, 0.0f, 1.0f);

    const vec4 lightColor1 = mix(dark_violet, chartreuse, abs(sin(0.0f)));
    const vec4 lightColor2 = vec4(1.0f, 1.0f, 1.0f, 1.0f);

    const vec4 lightColor = max(dot(normal.xyz, lightDir), 0.0f) * lightColor1;
    const float ambient = 0.05;
    // Light formula is pretty arbitrary and most definitely wrong
    outColor = (lightColor * shadow + ambient) * vec4(baseColor.xyz, 1.0f);
}