#include "WorldRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <glm/ext.hpp>
#include <imgui.h>

#include <iostream>

#include "stb_image.h"

WorldRenderer::WorldRenderer()
  : sceneMgr{std::make_unique<SceneManager>()}
{
}

void WorldRenderer::allocateResources(glm::uvec2 swapchain_resolution)
{
  resolution = swapchain_resolution;

  auto& ctx = etna::get_context();

  textureSampler = etna::Sampler{etna::Sampler::CreateInfo{
    .addressMode = vk::SamplerAddressMode::eMirroredRepeat, .name = "textureSampler"}};

  image = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "texture_image",
    .format = vk::Format::eB8G8R8A8Srgb,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment});

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
    .format = vk::Format::eR8G8B8A8Srgb,
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

  constants = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "constants",
  });

  constants.map();
}

void WorldRenderer::loadShaders()
{
  etna::create_program(
    "texture",
    {IMGUI_SHADERS_ROOT "texture.frag.spv",
     IMGUI_SHADERS_ROOT "toy.vert.spv"});

  etna::create_program(
    "imgui",
    {IMGUI_SHADERS_ROOT "toy.frag.spv", IMGUI_SHADERS_ROOT "toy.vert.spv"});
}

void WorldRenderer::setupPipelines(vk::Format swapchain_format)
{
 texturePipeline = etna::get_context().getPipelineManager().createGraphicsPipeline(
    "texture",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput = {
        .colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb},
      }});

  std::vector<vk::Format> swapchain_format_vector;
  swapchain_format_vector.push_back(swapchain_format);
  graphicsPipeline = etna::get_context().getPipelineManager().createGraphicsPipeline(
    "imgui",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput = {.colorAttachmentFormats = swapchain_format_vector}});
}

void WorldRenderer::update(FramePacket& FP)
{
  // calc camera matrix
  {
    const float aspect = float(resolution.x) / float(resolution.y);
    worldViewProj = FP.mainCam.projTm(aspect) * FP.mainCam.viewTm();
  }

  this->time = FP.time;
  this->yaw = FP.yaw;
  this->pitch = FP.pitch;
  this->mouse = FP.mouse;

  std::memcpy(constants.data(), &uniformParams, sizeof(uniformParams));
}

void WorldRenderer::renderWorld(vk::CommandBuffer cmd_buf,
                                vk::Image target_image, vk::ImageView target_image_view)
{
  // --- PASS 1: render to offscreen 'image' ---
  etna::set_state(cmd_buf, image.get(),
                  vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                  vk::AccessFlagBits2::eColorAttachmentWrite,
                  vk::ImageLayout::eColorAttachmentOptimal,
                  vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(cmd_buf);

  {
    etna::RenderTargetState rt1(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{ .image = image.get(), .view = image.getView({})}},
      {} );

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, texturePipeline.getVkPipeline());

    struct Params { glm::uvec2 res; float time; float scale; } params{resolution, time, scale};
    cmd_buf.pushConstants(texturePipeline.getVkPipelineLayout(),
                          vk::ShaderStageFlagBits::eFragment, 0, sizeof(params), &params);

    cmd_buf.draw(3, 1, 0, 0);
  }

  etna::set_state(cmd_buf, image.get(),
                  vk::PipelineStageFlagBits2::eFragmentShader,
                  vk::AccessFlagBits2::eShaderRead,
                  vk::ImageLayout::eShaderReadOnlyOptimal,
                  vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(cmd_buf);

  // --- PASS 2: render to swapchain, sample 'image' ---
  {
    etna::RenderTargetState rt2(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{ .image = target_image, .view = target_image_view }},
      {} );

    auto imguiInfo = etna::get_shader_program("imgui");
    auto set = etna::create_descriptor_set(
      imguiInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
        // binding 0 -> what has been rendered in PASS 1
        etna::Binding{ 0, image.genBinding(textureSampler.get(),
                                           vk::ImageLayout::eShaderReadOnlyOptimal) },
        // binding 1 -> PNG texture
        etna::Binding{ 1, texture.genBinding(textureSampler.get(),
                                             vk::ImageLayout::eShaderReadOnlyOptimal) },

        // binding 2 -> uniform buffer
        etna::Binding{ 2, constants.genBinding() }
      });

    vk::DescriptorSet vkSet = set.getVkSet();
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               graphicsPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);

    struct Params {
      glm::uvec2 res; glm::uvec2 mouse; float yaw; float pitch; float time; float planetSpeed;
    } params{resolution, mouse, yaw, pitch, time, planetSpeed};

    cmd_buf.pushConstants(graphicsPipeline.getVkPipelineLayout(),
                          vk::ShaderStageFlagBits::eFragment, 0, sizeof(params), &params);

    cmd_buf.draw(3, 1, 0, 0);
  }
}

void WorldRenderer::drawGui()
{
  ImGui::Begin("Simple render settings");

  ImGui::SliderFloat("Planet speed", &planetSpeed, 0.f, 5.0f);

  ImGui::SliderFloat("Surface texture scale", &scale, 0.f, 40.0f);

  float color[3]{uniformParams.baseColor.r, uniformParams.baseColor.g, uniformParams.baseColor.b};
  ImGui::ColorEdit3(
    "Meshes base color", color, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
  uniformParams.baseColor = {color[0], color[1], color[2]};

  ImGui::Text(
    "Application average %.3f ms/frame (%.1f FPS)",
    1000.0f / ImGui::GetIO().Framerate,
    ImGui::GetIO().Framerate);

  ImGui::NewLine();

  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Press 'B' to recompile and reload shaders");
  ImGui::End();
}
