#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "UniformParams.h"

layout(binding = 2, set = 0) uniform AppData { UniformParams uparams; };
layout(binding = 3) uniform sampler2D shadowMap;
layout(push_constant) uniform Push { 
    vec4 lightPos;
    float minFogDensity;
    float maxFogDensity;
    float baseLightLevel;
    float targetedLightCoeff;
    float fogSpeed;
} push;

layout(location = 0) out vec4 outColor;

float sampleShadow(vec3 worldPos) {
    vec4 lightSpace = uparams.lightVP * vec4(worldPos, 1.0);
    lightSpace /= lightSpace.w;
    vec3 projCoords = lightSpace.xyz * 0.5 + 0.5;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0) return 1.0;

    float shadowDepth = textureLod(shadowMap, projCoords.xy, 0).r;
    float bias = 0.01;
    return projCoords.z - bias > shadowDepth ? 0.0 : 1.0;
}

vec3 getViewRay(vec2 uv) {
    vec4 ndcNear = vec4(uv * 2.0 - 1.0, -1.0, 1.0);
    vec4 ndcFar  = vec4(uv * 2.0 - 1.0,  1.0, 1.0);

    vec4 worldNear = uparams.invViewProj * ndcNear;
    vec4 worldFar  = uparams.invViewProj * ndcFar;
    worldNear /= worldNear.w;
    worldFar  /= worldFar.w;

    return normalize(worldFar.xyz - worldNear.xyz);
}

float computeFogDensity(vec3 pos) {
    // напрямок "вітру" в площині XZ
    vec2 windDir = normalize(vec2(1.0, 0.3));
    float speed = 20; // швидкість руху

    // синусоїдальний рух
    float wave = sin(dot(pos.xz, windDir) * 0.2 + uparams.time * push.fogSpeed);

    // нормалізуємо в [0,1]
    wave = wave * 0.5 + 0.5;

    // мінімальна та максимальна щільність для видимого ефекту
    //float minDensity = 3.00;
    //float maxDensity = 5.00;

    // щільність, залежна від відстані до камери
    float viewDist = length(pos - uparams.camPos.xyz);
    float distanceFactor = smoothstep(0.0, 50.0, viewDist); // слабкий туман близько, сильніший далі

    return mix(push.minFogDensity, push.maxFogDensity, wave) * distanceFactor;
}

void main() {
    vec2 resolution = vec2(640.0, 360.0);
    vec2 uv = gl_FragCoord.xy / resolution;

    vec3 camPos = uparams.camPos.xyz;
    vec3 rayDir = getViewRay(uv);

    vec3 fogColor = vec3(0.6, 0.7, 0.8);
    //float fogDensity = push.fogDensity;
    int numSteps = 64;
    float maxDist = uparams.farPlane - uparams.nearPlane;
    float stepSize = maxDist / float(numSteps);

    vec3 accumLight = vec3(0.0);
    float transmittance = 1.0;

    for (int i = 0; i < numSteps; i++) {
        vec3 samplePos = camPos + rayDir * (float(i) * stepSize);
        vec3 toLight = push.lightPos.xyz - samplePos;
        float distToLight = length(toLight);
        vec3 lightDir = normalize(toLight);

        float fogDensity = computeFogDensity(samplePos);

        float shadowFactor = sampleShadow(samplePos);

        // експоненційне згасання від відстані до джерела
        float attenuation = exp(-0.05 * distToLight);

        float phase = max(dot(rayDir, lightDir), 0.0);
        //phase = pow(phase, 0.5); // м’яке падіння для god rays
        //if (phase < 0.9999) phase = 0.0;

        vec3 contrib = transmittance * shadowFactor * fogColor * attenuation * (push.baseLightLevel + phase * push.targetedLightCoeff) * stepSize;
        accumLight += contrib;
        accumLight = min(accumLight, vec3(0.4));

        // поглинання туманом
        transmittance *= exp(-fogDensity * stepSize);
    }

    // god rays без перетворення в біле
    outColor = vec4(accumLight, 1.0);
}
