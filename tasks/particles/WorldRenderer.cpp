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
  emitters.push_back(Emitter{
    .position = glm::vec3(0.0f, 0.0f, 0.2f),
    .spawnRate = 4.0f,          
    .particleLifetime = 0.1f,    
    .initialSpeed = 0.0f,        
    .particleList = {}
  });

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
    {PARTICLES_SHADERS_ROOT "texture.frag.spv",
     PARTICLES_SHADERS_ROOT "toy.vert.spv"});

  etna::create_program(
    "particles",
    {PARTICLES_SHADERS_ROOT "toy.frag.spv", PARTICLES_SHADERS_ROOT "toy.vert.spv"});

  etna::create_program(
  "emitters",
  {PARTICLES_SHADERS_ROOT "particles.frag.spv",
   PARTICLES_SHADERS_ROOT "particles.vert.spv"});

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
    "particles",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput = {.colorAttachmentFormats = swapchain_format_vector}});

  emittersPipeline = etna::get_context().getPipelineManager().createGraphicsPipeline("emitters", {
      .blendingConfig = {
          .attachments={
              vk::PipelineColorBlendAttachmentState{
                  .blendEnable = vk::True,
                  .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
                  .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
                  .colorBlendOp = vk::BlendOp::eAdd,
                  .srcAlphaBlendFactor = vk::BlendFactor::eOne,
                  .dstAlphaBlendFactor = vk::BlendFactor::eZero,
                  .alphaBlendOp = vk::BlendOp::eAdd,
                  .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
              },
          },
          .logicOpEnable = false,
          .logicOp = vk::LogicOp::eAnd,
          .blendConstants = {0, 0, 0, 0}
      },
      .fragmentShaderOutput =
      {
        .colorAttachmentFormats = {swapchain_format},
        .depthAttachmentFormat = vk::Format::eD32Sfloat,
      }
  });
}

void WorldRenderer::update(FramePacket& FP)
{
  // calc camera matrix
  {
    const float aspect = float(resolution.x) / float(resolution.y);
    worldViewProj = FP.mainCam.projTm(aspect) * FP.mainCam.viewTm();
    view = FP.mainCam.viewTm();
    cameraPos = FP.mainCam.position;
  }

  float deltaTime = FP.time - this->time;

  this->time = FP.time;
  this->yaw = FP.yaw;
  this->pitch = FP.pitch;
  this->mouse = FP.mouse;

  // Upload everything to GPU-mapped memory
  {
    float yOffsets[N_PLANETS] = {4.f, 6.f, 5.f, 7.f, 3.f};
    float speedModifiers[N_PLANETS] = {1.f, 2.f, 0.5f, 2.5f, 0.7f};

    for (int i = 0; i < N_PLANETS; ++i) {
        planets[i].orbitAngle += planetSpeed * deltaTime;

        uniformParams.planet[i] = glm::vec4(
            cos(planets[i].orbitAngle * speedModifiers[i]) * planets[i].radius,
            yOffsets[i] + sin(planets[i].orbitAngle * 0.2f) * planets[i].radius * 0.2f,
            sin(planets[i].orbitAngle * speedModifiers[i]) * planets[i].radius,
            i
        );
    }

    std::memcpy(constants.data(), &uniformParams, sizeof(uniformParams));
  }

  for (auto& emitter : emitters) {
    // спавн нових частинок
    spawnParticles(emitter, deltaTime);

    // оновлення існуючих
    for (auto& p : emitter.particleList) {
        p.age += deltaTime;
        if (p.age < p.lifetime) {
            p.pos += p.vel * deltaTime;
        }
    }

    // видалення "мертвих"
    emitter.particleList.erase(
        std::remove_if(emitter.particleList.begin(), emitter.particleList.end(),
                       [](auto& p) { return p.age >= p.lifetime; }),
        emitter.particleList.end());
  }

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

    auto particlesInfo = etna::get_shader_program("particles");
    auto set = etna::create_descriptor_set(
      particlesInfo.getDescriptorLayoutId(0),
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

    // --- PARTICLES ---
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, emittersPipeline.getVkPipeline());

    auto emittersInfo = etna::get_shader_program("emitters");
    auto emitterSet = etna::create_descriptor_set(
        emittersInfo.getDescriptorLayoutId(0),
        cmd_buf,
        {
            etna::Binding{ 0, texture.genBinding(textureSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) }
        }
    );

    vk::DescriptorSet emitterVkSet = emitterSet.getVkSet();
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               emittersPipeline.getVkPipelineLayout(), 0, 1, &emitterVkSet, 0, nullptr);

    struct PushConsts {
        glm::mat4 viewProj;
        glm::mat4 view;
        glm::vec4 camPos;
        glm::vec3 pos;
        float size;
        float alpha;
        float yaw;
        float pitch;
    };

    for (auto& emitter : emitters) {
        for (auto& p : emitter.particleList) {
            PushConsts pc{worldViewProj, view, glm::vec4(cameraPos, 1), p.pos, 1.0f, 1.0f - (p.age / p.lifetime), yaw, pitch};
            cmd_buf.pushConstants(emittersPipeline.getVkPipelineLayout(),
                                  vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                                  0, sizeof(PushConsts), &pc);

            cmd_buf.draw(6, 1, 0, 0);
        }
    }
  }
}

void WorldRenderer::drawGui()
{
  ImGui::Begin("Simple render settings");

  ImGui::SliderFloat("Planet speed", &planetSpeed, -20.0f, 20.0f);

  ImGui::SliderFloat("Surface texture scale", &scale, 0.f, 300.0f);

  float fov = uniformParams.fov;
  ImGui::SliderFloat("FOV", &fov, 0.1f, 10.0f);
  uniformParams.fov = fov;

  float spaceColor[3]{uniformParams.spaceColor.r, uniformParams.spaceColor.g, uniformParams.spaceColor.b};
  ImGui::ColorEdit3(
    "Cubemap Space Color", spaceColor, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
  uniformParams.spaceColor = {spaceColor[0], spaceColor[1], spaceColor[2]};

  float waveColor[3]{uniformParams.waveColor.r, uniformParams.waveColor.g, uniformParams.waveColor.b};
  ImGui::ColorEdit3(
    "Cubemap Wave Color", waveColor, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
  uniformParams.waveColor = {waveColor[0], waveColor[1], waveColor[2]};

  if (ImGui::Button("Default values")) {
    uniformParams.fov = 1.0f;
    planetSpeed = 1.0f;
    scale = 20.0f;
    uniformParams.spaceColor = {0.0f, 0.0f, 0.0f};
    uniformParams.waveColor = {0.15f, 0.75f, 0.03f};
  }

  if (ImGui::CollapsingHeader("Emitter")) {
    ImGui::SliderFloat3("Position", &emitters[0].position.x, -1.f, 1.f);
    ImGui::SliderFloat("Spawn rate", &emitters[0].spawnRate, 0.1f, 100.f);
    ImGui::SliderFloat("Lifetime", &emitters[0].particleLifetime, 0.1f, 10.f);
    ImGui::SliderFloat("Initial speed", &emitters[0].initialSpeed, 0.f, 10.f);
  }

  ImGui::Text(
    "Application average %.3f ms/frame (%.1f FPS)",
    1000.0f / ImGui::GetIO().Framerate,
    ImGui::GetIO().Framerate);

  ImGui::NewLine();

  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Press 'B' to recompile and reload shaders");
  ImGui::End();
}

void WorldRenderer::spawnParticles(Emitter& emitter, float deltaTime) {
    
    int count = static_cast<int>(100 * emitter.spawnRate * deltaTime);
    //std::cout << "emitter.spawnRate = " << emitter.spawnRate << std::endl;
    //std::cout << "deltaTime = " << deltaTime << std::endl;
    //std::cout << "Spawning " << count << " particles" << std::endl;

    for (int i = 0; i < count; i++) {
        Particle p;
        p.pos = emitter.position;
        p.vel = glm::sphericalRand(1.0f) * emitter.initialSpeed;
        p.lifetime = emitter.particleLifetime;
        p.age = 0.0f;

        emitter.particleList.push_back(p);
    }
}
