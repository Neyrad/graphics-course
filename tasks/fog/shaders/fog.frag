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

layout(push_constant) uniform Push {
    mat4 model;
    vec4 lightPos;
    uint id;
} push;

layout(location = 0) out vec4 outColor;

// ======================
// Допоміжна функція для перевірки тіні
// ======================
float sampleShadow(vec3 worldPos) {
    //vec4 lightSpace = (push.model * vec4(worldPos, 1.0));
    vec4 lightSpace = uparams.lightVP * vec4(worldPos, 1.0);
    lightSpace /= lightSpace.w;
    vec3 projCoords = lightSpace.xyz * 0.5 + 0.5;

    bool outOfView = (projCoords.x < 0.0 || projCoords.x > 1.0 ||
                      projCoords.y < 0.0 || projCoords.y > 1.0);
    if (outOfView) return 1.0;

    float shadowDepth = textureLod(shadowMap, projCoords.xy, 0).r;
    //return projCoords.z <= shadowDepth + 0.001 ? 1.0 : 0.0;
    float bias = 0.001;
    float shadow = projCoords.z - bias > shadowDepth ? 0.0 : 1.0;
    return shadow;
}

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

    // --------------------
    // Volumetric Fog (God Rays)
    // --------------------
    vec3 fogColor = vec3(0.6, 0.7, 0.8);
    float fogDensity = 0.05;   // регулюється через GUI
    int numSteps = 32;         // кількість кроків ray-march
    float stepSize = 1.0 / float(numSteps);

    vec3 rayDir = normalize(fragPos.xyz - uparams.camPos.xyz);
    float dist = length(fragPos.xyz - uparams.camPos.xyz);

    vec3 accumLight = vec3(0.0);
    float transmittance = 1.0;

    for (int i = 0; i < numSteps; i++) {
        float t = float(i) * stepSize;
        vec3 samplePos = uparams.camPos.xyz + rayDir * (t * dist);

        float shadowFactor = sampleShadow(samplePos);

        float localDensity = fogDensity;
        float absorption = exp(-localDensity * dist * stepSize);

        float phase = max(dot(rayDir, normalize(push.lightPos.xyz - samplePos)), 0.0);


        accumLight += transmittance * shadowFactor * localDensity * fogColor * phase;
        transmittance *= absorption;
    }

    float godRayIntensity = 9.0;
    vec3 finalColor = mix(sceneColor, fogColor + accumLight * godRayIntensity, 1.0 - transmittance);
    outColor = vec4(finalColor, 1.0);
}
/*
void main() {
    vec3 projCoords = (lightSpacePos.xyz / lightSpacePos.w) * 0.5 + 0.5;

    // якщо виходить за межі shadowMap, просто білий
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        outColor = vec4(1.0);
        return;
    }

    float depth = texture(shadowMap, projCoords.xy).r;

    // показати глибину як відтінок сірого
    outColor = vec4(depth, depth, depth, 1.0);
}
*/