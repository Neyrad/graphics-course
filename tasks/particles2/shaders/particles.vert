#version 450

#extension GL_GOOGLE_include_directive : require
#include "UniformParams.h"

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams uparams;
};

struct Particle {
    vec4 pos;
    vec4 vel;
    float lifetime;
    float age;
    float pad0;
    float pad1;
};

layout(std430, binding = 1) readonly buffer Particles {
    Particle particles[];
};
/*
layout(push_constant) uniform PushConsts {
    vec4 color;
    vec3 pos;
    float size;
    float alpha;
} pc;
*/
layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vUV;
layout(location = 2) out float vAlpha;

const vec2 quadVerts[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0),

    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0)
);

  const vec4 color = vec4(0, 0, 1, 1);
  const float size = 0.01;
  const float alpha = 1.0;

void main() {
    vec2 corner = quadVerts[gl_VertexIndex];

    vec3 camRight = vec3(uparams.view[0][0], uparams.view[1][0], uparams.view[2][0]);
    vec3 camUp    = vec3(uparams.view[0][1], uparams.view[1][1], uparams.view[2][1]);

    Particle p = particles[gl_InstanceIndex];
    vec3 worldPos = p.pos.xyz + (camRight * corner.x + camUp * corner.y) * size;

    gl_Position = uparams.viewProj * vec4(worldPos, 1.0);

    vUV = (corner + 1.0) * 0.5;
    vAlpha = alpha;
    vColor = color;
}
