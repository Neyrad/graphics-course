#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "UniformParams.h"

layout(location = 0) in vec4 inPos; // позиція вершини
layout(location = 1) in vec4 inNormal;

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

void main() {
    vec4 worldPos = uparams.model * vec4(inPos.xyz, 1.0);

    fragPos = worldPos;
    normal = vec4(mat3(uparams.model) * inNormal.xyz, 1.0);
    lightSpacePos = uparams.lightVP * worldPos; // для shadow map

    gl_Position = uparams.viewProj * worldPos;
//////////////////////////////////////////////////////////////////////////////////
    vec2 quadVerts[6] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 1.0, -1.0),
        vec2( 1.0,  1.0),
        vec2(-1.0, -1.0),
        vec2( 1.0,  1.0),
        vec2(-1.0,  1.0)
    );

    //vec3 pos = vec3(quadVerts[gl_VertexIndex], 0.0);
    
    vec3 pos = vertices[gl_VertexIndex].pos.xyz;
    gl_Position = uparams.viewProj * vec4(0.5*pos, 1.0);


    //gl_Position = vec4(pos*0.5, 0.0, 1.0);
}