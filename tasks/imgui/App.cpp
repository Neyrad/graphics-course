#include "App.hpp"
#include <iostream>

#include <tracy/Tracy.hpp>

#include "gui/ImGuiRenderer.hpp"


#include <algorithm> // for std::clamp

App::App()
{
  glm::uvec2 initialRes = {1280, 720};
  mainWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = initialRes,
  });

  renderer.reset(new Renderer(initialRes));

  auto instExts = windowing.getRequiredVulkanInstanceExtensions();
  renderer->initVulkan(instExts);

  auto surface = mainWindow->createVkSurface(etna::get_context().getInstance());

  renderer->initFrameDelivery(
    std::move(surface), [window = mainWindow.get()]() { return window->getResolution(); });

  // TODO: this is bad design, this initialization is dependent on the current ImGui context, but we
  // pass it implicitly here instead of explicitly. Beware if trying to do something tricky.
  ImGuiRenderer::enableImGuiForWindow(mainWindow->native());
}

App::~App()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::run()
{
  double lastTime = windowing.getTime();
  while (!mainWindow->isBeingClosed())
  {
    const double currTime = windowing.getTime();
    const float diffTime = static_cast<float>(currTime - lastTime);
    lastTime = currTime;

    windowing.poll();

    processInput(diffTime);

    drawFrame();
  }
  //ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::processInput(float dt)
{
  if (mainWindow.get()->keyboard[KeyboardKey::kB] == ButtonState::Falling)
  {
    const int retval = std::system("cd " GRAPHICS_COURSE_ROOT "/../build"
                                   " && cmake --build . --target imgui_shaders");
    if (retval != 0)
      spdlog::warn("Shader recompilation returned a non-zero return code!");
    else
    {
      ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
      etna::reload_shaders();
      spdlog::info("Successfully reloaded shaders!");
    }
  }

  if (mainWindow.get()->keyboard[KeyboardKey::kEscape] == ButtonState::Falling)
  {
    mainWindow.get()->askToClose();
  }

  if (is_held_down(mainWindow->keyboard[KeyboardKey::kLeftShift]))
    camMoveSpeed = 10;
  else
    camMoveSpeed = 1;

  if (mainWindow->mouse[MouseButton::mbRight] == ButtonState::Rising)
    mainWindow->captureMouse = !mainWindow->captureMouse;
  
  moveCam(mainCam, mainWindow->keyboard, dt);
  if (mainWindow->captureMouse)
    rotateCam(mainCam, mainWindow->mouse, dt);
}

void App::drawFrame()
{
  auto time = static_cast<float>(windowing.getTime());
  FramePacket FP;
  FP.mainCam = mainCam;
  FP.mouse = mouse;
  FP.yaw = yaw;
  FP.pitch = pitch;
  FP.time = time;

  renderer->update(FP);
  renderer->drawFrame();
}

void App::moveCam(Camera& cam, const Keyboard& kb, float dt)
{
  // Move position of camera based on WASD keys, and FR keys for up and down

  glm::vec3 dir = {0, 0, 0};

  if (is_held_down(kb[KeyboardKey::kS]))
    dir -= cam.forward();

  if (is_held_down(kb[KeyboardKey::kW]))
    dir += cam.forward();

  if (is_held_down(kb[KeyboardKey::kA]))
    dir -= cam.right();

  if (is_held_down(kb[KeyboardKey::kD]))
    dir += cam.right();

  if (is_held_down(kb[KeyboardKey::kF]))
    dir -= cam.up();

  if (is_held_down(kb[KeyboardKey::kR]))
    dir += cam.up();
/*
  std::cout << "dir = (" << dir[0] << ", " 
                         << dir[1] << ", "
                         << dir[2] << ")" << std::endl;

  std::cout << "normalize(dir) = (" << normalize(dir)[0] << ", " 
                         << normalize(dir)[1] << ", "
                         << normalize(dir)[2] << ")" << std::endl;
*/
  // NOTE: This is how you make moving diagonally not be faster than
  // in a straight line.
  cam.move(dt * camMoveSpeed * (length(dir) > 1e-9 ? normalize(dir) : dir));
}

void App::rotateCam(Camera& cam, const Mouse& ms, float /*dt*/)
{
  //std::cout << "ms.capturedPosDelta.x = " << ms.capturedPosDelta.x << std::endl;
  //std::cout << "ms.capturedPosDelta.y = " << ms.capturedPosDelta.y << std::endl;

  yaw -= ms.capturedPosDelta.x / 100.0f;
  pitch -= ms.capturedPosDelta.y / 100.0f;
  pitch = std::clamp(pitch, -1.5f, 1.5f);

  // Rotate camera based on mouse movement
  cam.rotate(camRotateSpeed * ms.capturedPosDelta.y, camRotateSpeed * ms.capturedPosDelta.x);

  // Increase or decrease field of view based on mouse wheel
  cam.fov -= zoomSensitivity * ms.scrollDelta.y;
  if (cam.fov < 1.0f)
    cam.fov = 1.0f;
  if (cam.fov > 120.0f)
    cam.fov = 120.0f;
}