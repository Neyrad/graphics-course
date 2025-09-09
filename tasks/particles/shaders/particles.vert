#version 450

#extension GL_GOOGLE_include_directive : require
#include "UniformParams.h"

layout(binding = 1, set = 0) uniform AppData
{
  UniformParams uparams;
};

layout(push_constant) uniform PushConsts {
    vec4 color;
    vec3 pos;        // позиція частинки у світі
    float size;      // розмір частинки
    float alpha;     // прозорість (передається у frag)
} pc;


layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vUV;
layout(location = 2) out float vAlpha;

// прості координати квадрата (-1..1)
const vec2 quadVerts[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0),

    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0)
);

void main() {
    vec2 corner = quadVerts[gl_VertexIndex];

    // Простий квадратик у world space
    float size = pc.size;
    //vec3 worldPos = pc.pos + vec3(corner * size, 0.0);



    // Беремо вектори "право" і "вгору" з view-матриці
    vec3 camRight = vec3(uparams.view[0][0], uparams.view[1][0], uparams.view[2][0]);
    vec3 camUp    = vec3(uparams.view[0][1], uparams.view[1][1], uparams.view[2][1]);

    // Будуємо вершину квадратика у world space
    vec3 worldPos = pc.pos + (camRight * corner.x + camUp * corner.y) * size;




    gl_Position = uparams.viewProj * vec4(worldPos, 1.0);

    vUV = (corner + 1.0) * 0.5;
    vAlpha = pc.alpha;
    vColor = pc.color;
}
