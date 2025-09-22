#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "UniformParams.h"

layout(location = 0) in vec4 fragPos;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec4 lightSpacePos;

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

vec4 debugColor = vec4(1.0, 0.0, 0.0, 1.0); 

vec4 leatherColor  = vec4(0.851, 0.729, 0.455, 1.0);
vec4 metalColor    = vec4(1.0, 1.0, 1.0, 1.0);
vec4 floorColor    = vec4(0.71, 0.518, 0.518, 1.0);
vec4 ceilingColor  = vec4(0.88, 0.87, 0.82, 1.0);
vec4 wallsColor    = vec4(0.72, 0.84, 0.75, 1.0); // (#B8D6BF)

void main() {

    vec4 baseColor;

    if (push.id == 0) {
      // floor
      baseColor = floorColor;
    } else if (push.id == 1) {
      // ceiling
      baseColor = ceilingColor;
    } else if (push.id >= 2 && push.id <= 5) {
      // walls
      baseColor = wallsColor;
    } else if (push.id >= 6 && push.id <= 7) {
      // seat and back
      baseColor = leatherColor;
    } else if (push.id >= 8 && push.id <= 13) {
      // metal chair parts
      baseColor = metalColor;
    } else {
      // debug
      baseColor = debugColor;
    }

    vec3 lightDir = normalize(push.lightPos.xyz - fragPos.xyz);

    vec3 projCoords = (lightSpacePos.xyz / lightSpacePos.w) * 0.5 + 0.5;
    bool outOfView = (projCoords.x < 0.0001 || projCoords.x > 0.9999 ||
                      projCoords.y < 0.0001 || projCoords.y > 0.9999);
    float shadow = ((projCoords.z < textureLod(shadowMap, projCoords.xy, 0).x + 0.001) || outOfView) ? 1.0 : 0.0;

    //vec4 dark_violet = vec4(0.59, 0.0, 0.82, 1.0);
    //vec4 chartreuse  = vec4(0.5, 1.0, 0.0, 1.0);
    vec4 lightColor1 = vec4(1.0);//mix(dark_violet, chartreuse, abs(sin(0.0)));
    vec4 lightColor  = max(dot(normal.xyz, lightDir), 0.0) * lightColor1;
    float ambient = 0.04;

    vec3 sceneColor = ((lightColor * shadow + ambient) * vec4(baseColor.xyz, 1.0)).rgb;

    vec2 screenResolution = {1280, 720};
    vec2 uv = gl_FragCoord.xy / screenResolution;
    vec3 fogTex = texture(fogHalfRes, uv).rgb; // тут апскейл
    vec3 finalColor = sceneColor + fogTex;
    outColor = vec4(finalColor, 1.0);
}