#version 450
#extension GL_GOOGLE_include_directive : require

#include "UniformParams.h"

layout(location = 0) in vec4 inPos;

layout(binding = 2, set = 0) uniform AppData
{
  UniformParams uparams;
};

void main() {
    gl_Position = uparams.lightVP * vec4(inPos.xyz, 1.0);
}
