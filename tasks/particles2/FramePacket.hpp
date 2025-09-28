#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <scene/Camera.hpp>

struct FramePacket
{
  Camera mainCam;
  glm::vec2 mouse;
  float yaw;
  float pitch;
  float time;
};
