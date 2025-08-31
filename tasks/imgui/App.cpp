#include "App.hpp"

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
  while (!mainWindow->isBeingClosed())
  {
    windowing.poll();
    processInput();
    drawFrame();
  }
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::processInput()
{
  if (mainWindow.get()->mouse[MouseButton::mbRight] == ButtonState::Rising)
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
    timer = std::chrono::system_clock::now();
  }

  if (mainWindow.get()->keyboard[KeyboardKey::kEscape] == ButtonState::Falling)
  {
    mainWindow.get()->askToClose();
  }

  if (mainWindow.get()->mouse[MouseButton::mbLeft] == ButtonState::High)
  {
    glm::uvec2 resolution = {1280, 720};

    float mouseUVx = mainWindow.get()->mouse.freePos.x / resolution.x;
    float mouseUVy = mainWindow.get()->mouse.freePos.y / resolution.y;

    float targetYaw =
      float(glm::mix(yaw + M_PI / 2.0f, yaw - M_PI / 2.0f, mouseUVx));
    float targetPitch =
      float(glm::mix(pitch + M_PI / 2.0f, pitch - M_PI / 2.0f, mouseUVy));

    // Smoothly interpolate to the target yaw and pitch
    yaw = glm::mix(yaw, targetYaw, 0.02f);
    pitch = glm::mix(pitch, targetPitch, 0.022f);

    // Limit pitch to avoid flipping
    pitch = std::clamp(pitch, -1.57f, 1.57f);

    mouse = mainWindow.get()->mouse.freePos;
  }
}

void App::drawFrame()
{
  renderer->update(static_cast<float>(windowing.getTime()));
  renderer->drawFrame();
}
