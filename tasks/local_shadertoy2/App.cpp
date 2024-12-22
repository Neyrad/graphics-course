#include "App.hpp"

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <chrono>
#include <etna/RenderTargetStates.hpp>
#include <etna/BlockingTransferHelper.hpp>

#include "stb_image.h"

#include <algorithm> // for std::clamp

App::App()
  : resolution{1280, 720}
  , useVsync{true}
  , timer{}
  , mouse{}
  , yaw{}
  , pitch{}
{
  {
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};
    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    etna::initialize(etna::InitParams{
      .applicationName = "Local Shadertoy",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      .physicalDeviceIndexOverride = {},
      .numFramesInFlight = 1,
    });
  }
  osWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = resolution,
  });

  {
    auto surface = osWindow->createVkSurface(etna::get_context().getInstance());

    vkWindow = etna::get_context().createWindow(etna::Window::CreateInfo{
      .surface = std::move(surface),
    });

    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    resolution = {w, h};
  }

  commandManager = etna::get_context().createPerFrameCmdMgr();
  context = &etna::get_context();

  etna::create_program(
    "texture",
    {LOCAL_SHADERTOY2_SHADERS_ROOT "texture.frag.spv",
     LOCAL_SHADERTOY2_SHADERS_ROOT "toy.vert.spv"});

  texturePipeline = etna::get_context().getPipelineManager().createGraphicsPipeline(
    "texture",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput = {
        .colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb},
      }});

  textureSampler = etna::Sampler{etna::Sampler::CreateInfo{
    .addressMode = vk::SamplerAddressMode::eMirroredRepeat, .name = "textureSampler"}};

  image = context->createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "texture_image",
    .format = vk::Format::eB8G8R8A8Srgb,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment});

  etna::create_program(
    "ls2",
    {LOCAL_SHADERTOY2_SHADERS_ROOT "toy.frag.spv", LOCAL_SHADERTOY2_SHADERS_ROOT "toy.vert.spv"});

  graphicsPipeline = context->getPipelineManager().createGraphicsPipeline(
    "ls2",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput = {.colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb}}});

  int texWidth, texHeight, texChannels;

  stbi_uc* pixels = stbi_load(
    GRAPHICS_COURSE_RESOURCES_ROOT "/textures/test_tex_1.png",
    &texWidth,
    &texHeight,
    &texChannels,
    STBI_rgb_alpha);

  VkDeviceSize imageSize = texWidth * texHeight * 4;

  if (!pixels)
  {
    throw std::runtime_error("failed to load texture image!");
  }

  texture = etna::get_context().createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1},
    .name = "texture",
    .format = vk::Format::eR8G8B8A8Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst});

  std::unique_ptr<etna::OneShotCmdMgr> oneShotCmdMgr = etna::get_context().createOneShotCmdMgr();

  auto blockingTransferHelper = etna::BlockingTransferHelper{
    etna::BlockingTransferHelper::CreateInfo{.stagingSize = static_cast<std::uint64_t>(imageSize)}};
  blockingTransferHelper.uploadImage(
    *oneShotCmdMgr,
    texture,
    0,
    0,
    std::span<const std::byte>(reinterpret_cast<const std::byte*>(pixels), imageSize));

  stbi_image_free(pixels);

  timer = std::chrono::system_clock::now();
}

App::~App()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::run()
{
  while (!osWindow->isBeingClosed())
  {
    windowing.poll();
    processInput();
    drawFrame();
  }
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::processInput()
{
  if (osWindow.get()->mouse[MouseButton::mbRight] == ButtonState::Rising)
  {
    const int retval = std::system("cd " GRAPHICS_COURSE_ROOT "/build"
                                   " && cmake --build . --target local_shadertoy2_shaders");
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

  if (osWindow.get()->keyboard[KeyboardKey::kEscape] == ButtonState::Falling)
  {
    osWindow.get()->askToClose();
  }

  constexpr double M_PI = 3.14159265;
  if (osWindow.get()->mouse[MouseButton::mbLeft] == ButtonState::High)
  {
    float mouseUVx = osWindow.get()->mouse.freePos.x / resolution.x;
    float mouseUVy = osWindow.get()->mouse.freePos.y / resolution.y;

    float targetYaw =
      float(glm::mix(yaw + M_PI / 2.0f, yaw - M_PI / 2.0f, mouseUVx));
    float targetPitch =
      float(glm::mix(pitch + M_PI / 2.0f, pitch - M_PI / 2.0f, mouseUVy));

    // Smoothly interpolate to the target yaw and pitch
    yaw = glm::mix(yaw, targetYaw, 0.02f);
    pitch = glm::mix(pitch, targetPitch, 0.022f);

    // Limit pitch to avoid flipping
    pitch = std::clamp(pitch, -1.57f, 1.57f);

    mouse = osWindow.get()->mouse.freePos;
  }
}

void App::drawFrame()
{
  auto currentCmdBuf = commandManager->acquireNext();
  etna::begin_frame();
  auto nextSwapchainImage = vkWindow->acquireNext();

  if (nextSwapchainImage)
  {
    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      auto curr_time = std::chrono::system_clock::now();

      etna::set_state(
        currentCmdBuf,
        image.get(),
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageAspectFlagBits::eColor);
      etna::flush_barriers(currentCmdBuf);


      {
        etna::RenderTargetState state(
          currentCmdBuf,
          {{}, {resolution.x, resolution.y}},
          {{image.get(), image.getView({})}},
          {});
        currentCmdBuf.bindPipeline(
          vk::PipelineBindPoint::eGraphics, texturePipeline.getVkPipeline());

        struct Params
        {
          glm::uvec2 res;
          float time;
        };
        Params params{resolution, std::chrono::duration<float>(curr_time - timer).count()};
        currentCmdBuf.pushConstants(
          texturePipeline.getVkPipelineLayout(),
          vk::ShaderStageFlagBits::eFragment,
          0,
          sizeof(params),
          &params);

        currentCmdBuf.draw(3, 1, 0, 0);
      }


      etna::set_state(
        currentCmdBuf,
        image.get(),
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::AccessFlagBits2::eShaderRead,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::ImageAspectFlagBits::eColor);
      etna::flush_barriers(currentCmdBuf);


      {
        etna::RenderTargetState state{
          currentCmdBuf, {{}, {resolution.x, resolution.y}}, {{backbuffer, backbufferView}}, {}};

        auto ls2Info = etna::get_shader_program("ls2");

        auto set = etna::create_descriptor_set(
          ls2Info.getDescriptorLayoutId(0),
          currentCmdBuf,
          {etna::Binding{
             0, image.genBinding(textureSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
           etna::Binding{
             1,
             texture.genBinding(textureSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}});

        vk::DescriptorSet vkSet = set.getVkSet();
        currentCmdBuf.bindPipeline(
          vk::PipelineBindPoint::eGraphics, graphicsPipeline.getVkPipeline());
        currentCmdBuf.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          graphicsPipeline.getVkPipelineLayout(),
          0,
          1,
          &vkSet,
          0,

          nullptr);

        struct Params
        {
          glm::uvec2 res;
          glm::uvec2 mouse;
          float yaw;
          float pitch;
          float time;
        };
        Params params{resolution, mouse, yaw, pitch, std::chrono::duration<float>(curr_time - timer).count()};
        currentCmdBuf.pushConstants(
          graphicsPipeline.getVkPipelineLayout(),
          vk::ShaderStageFlagBits::eFragment,
          0,
          sizeof(params),
          &params);

        currentCmdBuf.draw(3, 1, 0, 0);
      }

      etna::set_state(
        currentCmdBuf,
        backbuffer,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        {},
        vk::ImageLayout::ePresentSrcKHR,
        vk::ImageAspectFlagBits::eColor);

      etna::flush_barriers(currentCmdBuf);
    }
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());


    auto renderingDone =
      commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem));

    const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);

    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  etna::end_frame();

  if (!nextSwapchainImage && osWindow->getResolution() != glm::uvec2{0, 0})
  {
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }
}
