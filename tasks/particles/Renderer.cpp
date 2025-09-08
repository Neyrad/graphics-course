#include "Renderer.hpp"

#include <iostream>
#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/Profiling.hpp>
#include <imgui.h>

#include <gui/ImGuiRenderer.hpp>


Renderer::Renderer(glm::uvec2 res)
  : resolution{res}
{
}

void Renderer::initVulkan(std::span<const char*> instance_extensions)
{
  std::vector<const char*> instanceExtensions;

  for (auto ext : instance_extensions)
    instanceExtensions.push_back(ext);

  std::vector<const char*> deviceExtensions;

  deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  etna::initialize(etna::InitParams{
    .applicationName = "PARTICLES",
    .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
    .instanceExtensions = instanceExtensions,
    .deviceExtensions = deviceExtensions,
    .physicalDeviceIndexOverride = {},
  });
}

void Renderer::initFrameDelivery(vk::UniqueSurfaceKHR a_surface, ResolutionProvider res_provider)
{
  auto& ctx = etna::get_context();

  resolutionProvider = std::move(res_provider);
  commandManager = ctx.createPerFrameCmdMgr();

  window = ctx.createWindow(etna::Window::CreateInfo{
    .surface = std::move(a_surface),
  });

  auto [w, h] = window->recreateSwapchain(etna::Window::DesiredProperties{
    .resolution = {resolution.x, resolution.y},
    .vsync = true,
  });
  resolution = {w, h};

  worldRenderer = std::make_unique<WorldRenderer>();

  worldRenderer->allocateResources(resolution);
  worldRenderer->loadShaders();
  worldRenderer->setupPipelines(window->getCurrentFormat());

  guiRenderer = std::make_unique<ImGuiRenderer>(window->getCurrentFormat());
}

void Renderer::recreateSwapchain(glm::uvec2 res)
{
  auto& ctx = etna::get_context();

  ETNA_CHECK_VK_RESULT(ctx.getDevice().waitIdle());

  auto [w, h] = window->recreateSwapchain(etna::Window::DesiredProperties{
    .resolution = {res.x, res.y},
    .vsync = true,
  });
  resolution = {w, h};

  // Most resources depend on the current resolution, so we recreate them.
  worldRenderer->allocateResources(resolution);

  // Format of the swapchain CAN change on android
  worldRenderer->setupPipelines(window->getCurrentFormat());
}

void Renderer::loadScene()
{
  
}

void Renderer::update(FramePacket& FP)
{
  worldRenderer->update(FP);
}

void Renderer::drawFrame()
{
  //std::cout << "renderer draw frame run" << std::endl;
  //std::cout << "guirenderer nextframe" << std::endl;
  guiRenderer->nextFrame();
  //std::cout << "guirenderer nextframe DONE" << std::endl;
  ImGui::NewFrame();
  //std::cout << "worldrenderer drawgui" << std::endl;
  worldRenderer->drawGui();
  //std::cout << "worldrenderer drawgui DONE" << std::endl;
  ImGui::Render();

  //std::cout << "renderer draw frame run 1" << std::endl;

  auto currentCmdBuf = commandManager->acquireNext();
  etna::begin_frame();
  auto nextSwapchainImage = window->acquireNext();

  //std::cout << "renderer draw frame run 2" << std::endl;

  if (nextSwapchainImage)
  {
    //std::cout << "renderer draw frame run 21" << std::endl;

    auto [image, view, availableSem] = *nextSwapchainImage;

    //std::cout << "renderer draw frame run 22 " << std::endl;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      //ETNA_PROFILE_GPU(currentCmdBuf, renderFrame);
      //std::cout << "renderer draw frame run 221 " << std::endl;

      worldRenderer->renderWorld(currentCmdBuf, image, view);

      //std::cout << "renderer draw frame run 222 " << std::endl;

      {
        ImDrawData* pDrawData = ImGui::GetDrawData();
        guiRenderer->render(
          currentCmdBuf, {{0, 0}, {resolution.x, resolution.y}}, image, view, pDrawData);
      }

      //std::cout << "renderer draw frame run 223 " << std::endl;

      etna::set_state(
        currentCmdBuf,
        image,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        {},
        vk::ImageLayout::ePresentSrcKHR,
        vk::ImageAspectFlagBits::eColor);

      //std::cout << "renderer draw frame run 224 " << std::endl;
      etna::flush_barriers(currentCmdBuf);
      //std::cout << "renderer draw frame run 225 " << std::endl;
      //ETNA_READ_BACK_GPU_PROFILING(currentCmdBuf);
    }
    //std::cout << "renderer draw frame run 23" << std::endl;
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());
    //std::cout << "renderer draw frame run 24" << std::endl;

    auto renderingDone =
      commandManager->submit(std::move(currentCmdBuf), std::move(availableSem));

    //std::cout << "renderer draw frame run 25" << std::endl;

    const bool presented = window->present(std::move(renderingDone), view);

    //std::cout << "renderer draw frame run 26" << std::endl;

    if (!presented)
      nextSwapchainImage = std::nullopt;

    //std::cout << "renderer draw frame run 27" << std::endl;
  }

  //std::cout << "renderer draw frame run 3" << std::endl;

  etna::end_frame();

  //std::cout << "renderer draw frame run 4" << std::endl;

  if (!nextSwapchainImage)
  {
    auto res = resolutionProvider();
    // On windows, we get 0,0 while the window is minimized and
    // must skip frames until the window is un-minimized again
    if (res.x != 0 && res.y != 0)
      recreateSwapchain(res);
  }

  //std::cout << "renderer draw frame finish" << std::endl;
}

Renderer::~Renderer()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}
