#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "UniformParams.h"

layout(location = 0) in vec4 fragPos;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec4 lightSpacePos;
layout(location = 3) in vec4 coolColor;
layout(location = 4) in vec4 skyboxColor;

layout(binding = 2, set = 0) uniform AppData
{
  UniformParams uparams;
};

layout(binding = 3) uniform sampler2D shadowMap;

layout(binding = 5) uniform sampler2D fogHalfRes;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 lightPos;
    uint id;
} push;

layout(location = 0) out vec4 outColor;

void main() {
    // --------------------
    // Базове освітлення
    // --------------------
    vec3 lightDir = normalize(push.lightPos.xyz - fragPos.xyz);
    vec4 baseColor = push.id == 1 ? skyboxColor : coolColor;

    vec3 projCoords = (lightSpacePos.xyz / lightSpacePos.w) * 0.5 + 0.5;
    bool outOfView = (projCoords.x < 0.0001 || projCoords.x > 0.9999 ||
                      projCoords.y < 0.0001 || projCoords.y > 0.9999);
    float shadow = ((projCoords.z < textureLod(shadowMap, projCoords.xy, 0).x + 0.001) || outOfView) ? 1.0 : 0.0;

    vec4 dark_violet = vec4(0.59, 0.0, 0.82, 1.0);
    vec4 chartreuse  = vec4(0.5, 1.0, 0.0, 1.0);
    vec4 lightColor1 = mix(dark_violet, chartreuse, abs(sin(0.0)));
    vec4 lightColor  = max(dot(normal.xyz, lightDir), 0.0) * lightColor1;
    float ambient = 0.04;

    vec3 sceneColor = ((lightColor * shadow + ambient) * vec4(baseColor.xyz, 1.0)).rgb;

    vec2 screenResolution = {1280, 720};
    vec2 uv = gl_FragCoord.xy / screenResolution;
    vec3 fogTex = texture(fogHalfRes, uv).rgb; // тут апскейл
    vec3 finalColor = sceneColor + fogTex;
    outColor = vec4(finalColor, 1.0);
}