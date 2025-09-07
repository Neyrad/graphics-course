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

mat3 rotateX(float theta)
{
    float c = cos(theta);
    float s = sin(theta);
    return mat3(
        vec3(1, 0, 0),
        vec3(0, c, -s),
        vec3(0, s, c)
    );
}

mat3 rotateY(float theta)
{
    float c = cos(theta);
    float s = sin(theta);
    return mat3(
        vec3(c, 0, s),
        vec3(0, 1, 0),
        vec3(-s, 0, c)
    );
}

void main() {

    vec3 cameraPosition = vec3(0, 0, 5);
    vec3 lookAt = vec3(0, 0, 0); // Look at the origin
    
    // Apply rotation to camera position
    mat3 rotX = rotateX(pc.pitch);
    mat3 rotY = rotateY(pc.yaw);
    vec3 rotatedCamera = rotY * rotX * (cameraPosition - lookAt) + lookAt;

    vec2 corner = quadVerts[gl_VertexIndex];

    // напрямок від камери до частинки
    vec3 look = normalize(pc.pos - rotatedCamera);

    // правий і верхній вектори у world space
    //vec3 right = normalize(cross(vec3(0,1,0), look));
    //vec3 up    = normalize(cross(look, right));

    vec3 right = vec3(1,0,0); // світова X-вісь
    vec3 up    = vec3(0,1,0); // світова Y-вісь

    // можна крутити квадратиком навколо осі погляду
    float angle = 0;
    float ca = cos(angle), sa = sin(angle);
    vec3 r = right * ca + up * sa;
    vec3 u = up * ca - right * sa;

    // позиція вершини у світі
    vec3 worldPos = pc.pos + (r * corner.x + u * corner.y) * 0.02;

    //gl_Position = pc.view * vec4(worldPos, 1.0);
    gl_Position = vec4(worldPos, 1.0);

    vUV = (corner + 1.0) * 0.5;
    vAlpha = pc.alpha;
}
