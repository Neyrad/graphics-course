#ifndef UNIFORM_PARAMS_H_INCLUDED
#define UNIFORM_PARAMS_H_INCLUDED

#include "cpp_glsl_compat.h"

#define N_PLANETS 5

struct UniformParams
{
  shader_vec4 planet[N_PLANETS];
  shader_vec3 spaceColor;
  float pad1;
  shader_vec3 waveColor;
  float pad2;
};


#endif // UNIFORM_PARAMS_H_INCLUDED
