#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler2D iChannel1;
layout(binding = 1) uniform sampler2D iChannel2;

layout(push_constant) uniform params {
  uvec2 iResolution;
  uvec2 iMouse;
  float yaw;
  float pitch;
  float iTime;
};

const vec3 light = vec3(0, 6, 5);
const float fov = 1.0; // Wider FOV for a better perspective
const int maxSteps = 70;
const float eps = 0.01;
const float maxDist = 100.0;

mat3 camera(vec3 eye, vec3 lookAt, vec3 up)
{
    vec3 forward    = normalize(lookAt - eye);       // Camera forward direction
    vec3 right      = normalize(cross(up, forward)); // Camera right direction
    vec3 upAdjusted = cross(forward, right);         // Camera up direction
    
    return mat3(right, upAdjusted, -forward);
}

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

float sdThickDisk(in vec3 p, in vec3 n, in float r, in float thick, in vec3 pos)  
{
    vec3 d = dot(p - pos, n) * n;
    vec3 o = (p - pos) - d;
    o -= normalize(o) * min(length(o), r);
    return length(d + o) - thick;
}

float dSphere(vec3 p, float r, vec3 pos)
{
    return length(p - pos) - r;
}

float opSubtraction(float d1, float d2)
{
    return max(-d1, d2);
}

float dRing(in vec3 q, float inR, float outR, in vec3 pos)
{
    float disk = sdThickDisk(q, vec3(0.0, 1.0, 0.0), outR, 0.01, pos);
    float ring = opSubtraction(dSphere(q - vec3(0.0, 0.0, 0.0), inR, pos), disk);
    return ring;
}

const float INF = 1e9;
vec3 dRings(in vec3 q, int n, float dist, float shift, in vec3 pos, float id)
{
    float step = dist * (1.0 / float(n));
    float ret = INF;
    float curRing = 0.0;
    for (int i = 0; i < n; ++i)
    {
        float inR  = shift + step * float(i);
        float outR = shift + step * (float(i) + 0.5);
        float ring = dRing(q, inR, outR, pos);
        if (ring < ret)
        {
            ret = ring;
            curRing = float(i);
        }
        
    }
    
    return vec3(ret, curRing, id * 10.);
}

vec3 saturnSdf(in vec3 p, in vec2 uv, in mat3 m, in vec4 pos, float id)
{
    vec3 q = m * p;

    // Ring
    int nRings = int(pos.w);
    vec3 ringsSdf = dRings(q, nRings, 2.0, 1.5, pos.xyz, id);
    
    // Sphere
    float sphereSdf = dSphere(q, 1.1, pos.xyz);
    
    if (ringsSdf.x < sphereSdf)
    {
        return ringsSdf;
    }
    else return vec3(sphereSdf, 0, id);
}

#define N_PLANETS 5
vec3 sdf(in vec3 p, in vec2 uv, in mat3 m, out vec4 planet[N_PLANETS])
{
    float orbitRadius = 9.0;
    float orbitSpeed = iTime * (1. / 5.);
    float coss = orbitRadius * cos(orbitSpeed);
    float sinn = orbitRadius * sin(orbitSpeed);
    float mcoss = coss * (1. / 5.);
    float msinn = sinn * (1. / 5.);
    planet[0] = vec4(2.*coss,   5.+mcoss,  sinn,      3.);
    planet[1] = vec4(-coss,     -3.+mcoss, -sinn,     1.);
    planet[2] = vec4(3.*sinn,   msinn,     coss,      2.);
    planet[3] = vec4(1.+coss,   -5.+msinn, 2.*sinn,   2.);
    planet[4] = vec4(-2.-mcoss, -3.+mcoss, -1.5*coss, 0.);

    vec3 ret = vec3(INF, 0, 0);
    int id = 0;
    for (int i = 0; i < N_PLANETS; ++i)
    {
        vec3 saturn = saturnSdf(p, uv, m, planet[i], float(id));
        if (saturn.x < ret.x) ret = saturn;    
        ++id;
    }
    
    return ret;
}

vec3 trace(in vec2 uv, in vec3 from, in vec3 dir, out bool hit, out float id, in mat3 m, out vec4 planet[N_PLANETS])
{
    vec3 p = from;
    float totalDist = 0.0;
    id = -1.0;
    hit = false;

    for (int steps = 0; steps < maxSteps; ++steps)
    {
        vec3 distAndID = sdf(p, uv, m, planet);
        float dist = abs(distAndID.x);
        
        if (dist < eps)
        {
            hit = true;
            id = distAndID.z; 
            break;
        }
        
        totalDist += dist;
        if (totalDist > maxDist) break;
            
        p += dist * dir;
    }
    
    return p;
}

vec3 generateNormal(in vec2 uv, vec3 z, float d, in mat3 m, in vec4 planet[N_PLANETS])
{
    float e = max(d * 0.5, eps);
    float dx1 = sdf(z + vec3(e, 0, 0), uv, m, planet).x;
    float dx2 = sdf(z - vec3(e, 0, 0), uv, m, planet).x;
    float dy1 = sdf(z + vec3(0, e, 0), uv, m, planet).x;
    float dy2 = sdf(z - vec3(0, e, 0), uv, m, planet).x;
    float dz1 = sdf(z + vec3(0, 0, e), uv, m, planet).x;
    float dz2 = sdf(z - vec3(0, 0, e), uv, m, planet).x;

    return normalize(vec3(dx1 - dx2, dy1 - dy2, dz1 - dz2));
}

vec3 triplanarProjection(vec3 normal, vec3 worldPos)
{
    // Normalize the world position to fit within 0-1 range
    vec3 scaledPos = fract(worldPos * 0.2); // Scale and repeat the texture over large objects
   
    // Triplanar blending
    vec3 blending = abs(normal);
    blending = (blending - 0.2) * 2.0;
    blending = max(blending, 0.0);
    blending /= (blending.x + blending.y + blending.z);

    // Sample the texture from each axis
    vec3 texX = texture(iChannel2, scaledPos.yz).rgb;
    vec3 texY = texture(iChannel2, scaledPos.xz).rgb;
    vec3 texZ = texture(iChannel2, scaledPos.xy).rgb;

    return texX * blending.x + texY * blending.y + texZ * blending.z;
}

vec2 sphericalUV(vec3 position)
{
    // Calculate spherical coordinates
    float longitude = atan(position.z, position.x); // Range [-π, π]
    float latitude = acos(position.y / length(position)); // Range [0, π]

    // Map to UV space (longitude: [0, 1], latitude: [0, 1])
    vec2 uv;
    uv.x = (longitude / (2.0 * 3.14159265)) + 0.5; // Longitude mapped to [0, 1]
    uv.y = latitude / 3.14159265; // Latitude mapped to [0, 1]

    return uv;
}

vec3 triplanarProjectionSphere(vec3 normal, vec3 worldPos)
{
    // Use spherical UV mapping instead of triplanar blending
    vec2 uv = sphericalUV(worldPos);

    // Sample texture using the spherical UV coordinates
    vec3 textureColor = texture(iChannel1, uv).rgb;

    return textureColor;
}

float max3(vec3 rd)
{
    return max(max(rd.x, rd.y), rd.z);
}

float rand(float co) { return fract(sin(co*(91.3458)) * 47453.5453); }
float rand(vec2 co){ return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453); }
float rand(vec3 co){ return rand(co.xy+rand(co.z)); }

vec4 Cubemap(in vec2 fragCoord, in vec3 rayDir)
{
    // Normalize ray direction
    vec3 rd = normalize(rayDir);

    // Define base colors
    vec3 spaceColor = vec3(0.0, 0.0, 0.0); // Dark background (space)
    vec3 waveColor = vec3(0.15, 0.75, 0.03);  // Wave color

    // Base color for space
    vec3 col = spaceColor;

    // Calculate wave movement based on time and direction
    float waveSpeed = 0.5; // Speed of the wave movement
    vec3 waveDirection = normalize(vec3(0.0, -1.0, 0.0)); // Direction of the waves in 3D space

    // Calculate a repeating pattern of stripes using the dot product
    // This creates periodic stripes based on ray direction and time
    float stripeWidth = 0.1; // Width of each stripe (adjust for more or fewer stripes)
    float wavePattern = sin(dot(rd, waveDirection) * 20 + iTime * waveSpeed); // Higher multiplier = more stripes

    // Apply a power function to make the stripes sharper and more intense
    float stripes = pow(abs(wavePattern), 3.0); // Increase the power for more intense stripes

    // Define intensity threshold to make the stripes sharp and avoid soft transitions
    float intensityThreshold = 0.5; // Stripes will appear above this threshold
    stripes = step(intensityThreshold, stripes); // Use step() to create sharp transitions

    // Apply the stripes to the background color
    col = mix(col, waveColor, stripes);

    // Return the final color with the wave stripes effect
    return vec4(col, 1.0);
}

void main() {
    bool hit;
    float id;

    // Shader setup
    vec2 uv = (vec2(gl_FragCoord) - 0.5 * iResolution.xy) / iResolution.y;

    // Set camera position and look-at target
    vec3 cameraPosition = vec3(0, 0, 5);
    vec3 lookAt = vec3(0, 0, 0); // Look at the origin
    
    vec2 Mouse = vec2(iMouse) / vec2(iResolution);
    
    // Apply rotation to camera position
    mat3 rotX = rotateX(pitch);
    mat3 rotY = rotateY(yaw);
    vec3 rotatedCamera = rotY * rotX * (cameraPosition - lookAt) + lookAt;
    
    // Create camera matrix
    mat3 camMat = camera(rotatedCamera, lookAt, vec3(0, 1, 0));
    
    // Compute ray direction
    vec3 rayDir = camMat * normalize(vec3(uv * fov, -1.0)); // Adjust FOV

    // Trace rays and objects in the scene
    vec4 planet[N_PLANETS];
    vec3 p = trace(uv, rotatedCamera, rayDir, hit, id, mat3(1.0), planet); // Apply object rotation (identity matrix for now)
    
    vec3 color = Cubemap(uv, rayDir).rgb;

    if (hit)
    {
        vec3 objColor = vec3(0.0); // Default object color
        
        vec3 normal     = generateNormal(uv, p, 0.001, mat3(1.0), planet);
        vec3 lightDir   = normalize(light - p);
        vec3 viewDir    = normalize(rotatedCamera - p);
        vec3 halfwayDir = normalize(lightDir + viewDir);
        
        // Simple lighting
        float diff = max(dot(normal, lightDir), 0.0);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
        
        if (id < 10.)
        {
            objColor = triplanarProjectionSphere(normal, p - planet[int(id)].xyz);
        }
        else          
        {
            id = id * (1. / 10.);
            objColor = triplanarProjection(normal, p - planet[int(id)].xyz);
        }
        color = objColor * diff + vec3(1.0) * spec;
    }

    outColor = vec4(color, 1.0);
}