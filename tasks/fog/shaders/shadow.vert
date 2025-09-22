#version 450
#extension GL_GOOGLE_include_directive : require

#include "UniformParams.h"

layout(binding = 1, set = 0) uniform AppData
{
  UniformParams uparams;
};

layout(push_constant) uniform Push {
    mat4 model;
} push;

struct Vertex {
  vec4 pos;
  vec4 normal;
};

layout(binding = 2) readonly buffer Vertices {
    Vertex vertices[];
};

void main() {
    vec3 pos = vertices[gl_VertexIndex].pos.xyz;
    vec4 worldPos = push.model * vec4(pos, 1.0);
    gl_Position = uparams.lightVP * vec4(worldPos.xyz, 1.0);
}