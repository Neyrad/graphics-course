#ifndef UNIFORM_PARAMS_H_INCLUDED
#define UNIFORM_PARAMS_H_INCLUDED

#include "cpp_glsl_compat.h"

struct UniformParams
{
  shader_mat4 lightVP;
  shader_mat4 viewProj;
  shader_mat4 invViewProj;
  shader_mat4 view;
  shader_vec4 camPos;
  float nearPlane;
  float farPlane;
};

#endif // UNIFORM_PARAMS_H_INCLUDED
