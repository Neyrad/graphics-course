#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform params {
  uvec2 iResolution;
  float iTime;
};

void main()
{
    // Normalize UV coordinates to range [0, 1]
    vec2 uv = vec2(gl_FragCoord).xy / iResolution.xy;

    // Scale the UVs to adjust the size of the triangles
    float scale = 20.0;
    uv *= scale;

    // Calculate triangular grid
    float x = uv.x;
    float y = uv.y;

    // Determine triangle row and column
    int row = int(floor(y));
    int col = int(floor(x));

    // Alternate between triangles pointing up and down based on row and column
    bool pointingUp = (col + row) % 2 == 0;

    // Calculate the local position within the cell
    vec2 localPos = fract(uv);

    // Check whether the current pixel is inside the triangle
    float edge = localPos.x + (pointingUp ? localPos.y : 1.0 - localPos.y);

    // Assign colors based on position inside the triangle
    vec3 color1 = vec3(0.6, 0.4, 0.7); // Blue-ish
    vec3 color2 = vec3(0.9, 0.8, 0.5); // Yellow-ish

    // Choose between two colors based on the triangle orientation
    vec3 color = edge < 1.0 ? color1 : color2;

    // Output the final color
    outColor = vec4(color, 1.0);
}