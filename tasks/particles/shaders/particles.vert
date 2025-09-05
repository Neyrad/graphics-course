#version 450

layout(push_constant) uniform PushConsts {
    mat4 viewProj;   // матриця камери
    vec3 pos;        // позиція частинки у світі
    float size;      // розмір частинки
    float alpha;     // прозорість (передається у frag)
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

    // billboard: частинка завжди фронтально до камери
    // беремо правий/верхній вектори з view-матриці
    vec3 right = vec3(pc.viewProj[0][0], pc.viewProj[1][0], pc.viewProj[2][0]);
    vec3 up    = vec3(pc.viewProj[0][1], pc.viewProj[1][1], pc.viewProj[2][1]);

    vec3 worldPos = pc.pos + (right * corner.x + up * corner.y) * pc.size;

    gl_Position = pc.viewProj * vec4(worldPos, 1.0);

    vUV = (corner + 1.0) * 0.5; // UV від (0,0) до (1,1)
    vAlpha = pc.alpha;
}
