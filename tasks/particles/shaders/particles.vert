#version 450

layout(push_constant) uniform PushConsts {
    mat4 viewProj;   // матриця камери
    mat4 view;
    vec4 camPos;
    vec3 pos;        // позиція частинки у світі
    float size;      // розмір частинки
    float alpha;     // прозорість (передається у frag)
    float yaw;
    float pitch;
} pc;


layout(location = 0) out vec2 vUV;
layout(location = 1) out float vAlpha;

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
    vec3 worldPos = pc.pos + vec3(corner * pc.size, 0.0);

    gl_Position = pc.viewProj * vec4(worldPos, 1.0);

    vUV = (corner + 1.0) * 0.5;
    vAlpha = pc.alpha;
}
