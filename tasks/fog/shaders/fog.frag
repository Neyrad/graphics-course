#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 fragPos;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec4 lightSpacePos;
layout(location = 3) in vec4 coolColor;
layout(location = 4) in vec4 skyboxColor;

//layout(binding = 3) uniform sampler2DShadow shadowMap;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 lightPos;
    uint id;
} push;

layout(location = 0) out vec4 outColor;
/*
// простий shadow calculation
float shadow(vec4 lightSpacePos) {
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;
    return texture(shadowMap, projCoords);
}
*/
void main() {
    vec3 lightDir = normalize(push.lightPos.xyz - fragPos.xyz);
    float diff = max(dot(normalize(normal.xyz), lightDir), 0.0);
    
    float shadowFactor = 1.0;//shadow(lightSpacePos); // від shadow map

    vec3 color = coolColor.xyz * diff * shadowFactor;
    //outColor = vec4(color, 1.0);
    outColor = push.id == 0 ? coolColor : skyboxColor;
}
