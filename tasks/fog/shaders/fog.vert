#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "UniformParams.h"

layout(binding = 2, set = 0) uniform AppData
{
  UniformParams uparams;
};

struct Vertex {
  vec4 pos;
  vec4 normal;
};

layout(std430, binding = 4) readonly buffer Vertices {
    Vertex vertices[];
};

layout(location = 0) out vec4 fragPos;
layout(location = 1) out vec4 normal;
layout(location = 2) out vec4 lightSpacePos;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 lightPos;
    uint id;
} push;

void main() {
    vec3 pos = vertices[gl_VertexIndex].pos.xyz;
    vec4 worldPos = push.model * vec4(pos, 1.0);

    fragPos = worldPos;
    normal = vec4(mat3(push.model) * vertices[gl_VertexIndex].normal.xyz, 1.0);
    lightSpacePos = uparams.lightVP * worldPos;

    gl_Position = uparams.viewProj * vec4(worldPos.xyz, 1.0);
}