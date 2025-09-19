#pragma once

#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>

#include "wsi/OsWindowingManager.hpp"
#include "scene/Camera.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/Sampler.hpp>

#include "Renderer.hpp"
#include "FramePacket.hpp"

class App
{
public:
  App();
  ~App();

  void run();

private:
  void drawFrame();
  void processInput(float dt);

  void moveCam(Camera& cam, const Keyboard& kb, float dt);
  void rotateCam(Camera& cam, const Mouse& ms, float dt);

private:
  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> mainWindow;
  
  float camMoveSpeed = 1;
  float camRotateSpeed = 0.1f;
  float zoomSensitivity = 2.0f;
  Camera mainCam;

  glm::vec2 mouse;
  float yaw;
  float pitch;

  std::unique_ptr<Renderer> renderer;
};
