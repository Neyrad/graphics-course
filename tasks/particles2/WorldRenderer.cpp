#include "WorldRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <etna/Buffer.hpp>
#include <glm/ext.hpp>
#include <imgui.h>

#include <iostream>
#include <numeric>

#include "stb_image.h"

const uint32_t maxParticles = 100000;

WorldRenderer::WorldRenderer()
  : sceneMgr{std::make_unique<SceneManager>()}
{
  lightPos = glm::vec3(0.0f, -6.0f, -5.0f);

  particleBuffer = etna::get_context().createBuffer(etna::Buffer::CreateInfo{
      .size = sizeof(Particle) * maxParticles,
      .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
      .name = "particleBuffer",
  });

  indirectBuffer = etna::get_context().createBuffer(etna::Buffer::CreateInfo{
      .size = sizeof(VkDrawIndirectCommand) * maxParticles,
      .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
      .name = "indirectBuffer",
  });

  counterBuffer = etna::get_context().createBuffer(etna::Buffer::CreateInfo{
      .size = sizeof(uint32_t), // один int для aliveCount
      .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
      .name = "counterBuffer",
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
    {PARTICLES2_SHADERS_ROOT "texture.frag.spv",
     PARTICLES2_SHADERS_ROOT "toy.vert.spv"});

  etna::create_program(
    "particles2",
    {PARTICLES2_SHADERS_ROOT "toy.frag.spv", PARTICLES2_SHADERS_ROOT "toy.vert.spv"});

  etna::create_program(
  "emitters",
  {PARTICLES2_SHADERS_ROOT "particles.frag.spv",
   PARTICLES2_SHADERS_ROOT "particles.vert.spv"});

  etna::create_program(
    "simulate",
    { PARTICLES2_SHADERS_ROOT "simulate.comp.spv" }
  );

  etna::create_program(
    "spawn",
    { PARTICLES2_SHADERS_ROOT "spawn.comp.spv" }
  );

  etna::create_program(
    "writeIndirect",
    { PARTICLES2_SHADERS_ROOT "writeIndirect.comp.spv" }
  );

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
    "particles2",
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

  simulatePipeline = etna::get_context().getPipelineManager().createComputePipeline(
    "simulate",
    etna::ComputePipeline::CreateInfo{}
);

spawnPipeline = etna::get_context().getPipelineManager().createComputePipeline(
    "spawn",
    etna::ComputePipeline::CreateInfo{}
);

writeIndirectPipeline = etna::get_context().getPipelineManager().createComputePipeline(
    "writeIndirect",
    etna::ComputePipeline::CreateInfo{}
);



}

#include <glm/gtx/string_cast.hpp>



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
/*
  for (auto& emitter : emitters) {
    spawnParticles(emitter, deltaTime);

    for (auto& p : emitter.particleList) {
        p.age += deltaTime;
        if (p.age < p.lifetime) {
            p.pos += p.vel * deltaTime;
        }
    }

    emitter.particleList.erase(
        std::remove_if(emitter.particleList.begin(), emitter.particleList.end(),
                       [](auto& p) { return p.age >= p.lifetime; }),
        emitter.particleList.end());

    std::sort(emitter.particleList.begin(), emitter.particleList.end(),
              [&](const Particle& a, const Particle& b){
                  float da = glm::distance(a.pos, cameraPos);
                  float db = glm::distance(b.pos, cameraPos);
                  return da > db;
              });
  }
*/
}

void WorldRenderer::renderWorld(vk::CommandBuffer cmd_buf,
                                vk::Image target_image, vk::ImageView target_image_view)
{

  
  ///
  ///
  /// COMPUTE PART
  ///
  ///


  // SPAWN
  cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, spawnPipeline.getVkPipeline());
  auto spawnInfo = etna::get_shader_program("spawn");
  auto spawnSet = etna::create_descriptor_set(
      spawnInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
          etna::Binding{1, particleBuffer.genBinding()},   // твій SSBO з частинками
          etna::Binding{2, counterBuffer.genBinding()}         // якісь uniform-константи
      }
  );
  vk::DescriptorSet spawnVkSet = spawnSet.getVkSet();
  cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                            spawnPipeline.getVkPipelineLayout(),
                            0, 1, &spawnVkSet, 0, nullptr);

  struct SpawnPush {
      uint32_t spawnCount;
      glm::vec3 emitterPos;
      float life;
  } pushParams_spawn { 6000, glm::vec3(0, 0, 0), 10.0f };

  cmd_buf.pushConstants(
      spawnPipeline.getVkPipelineLayout(),
      vk::ShaderStageFlagBits::eCompute,
      0,
      sizeof(pushParams_spawn),
      &pushParams_spawn
  );
  
  uint32_t workgroupSize_spawn = 64;
  uint32_t numGroups_spawn = (maxParticles + workgroupSize_spawn - 1) / workgroupSize_spawn;
  cmd_buf.dispatch(numGroups_spawn, 1, 1);


  // SIMULATE
  cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, simulatePipeline.getVkPipeline());
  auto simInfo = etna::get_shader_program("simulate");
  auto simSet = etna::create_descriptor_set(
      simInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
          etna::Binding{0, particleBuffer.genBinding()},
          etna::Binding{1, particleBuffer.genBinding()},
          etna::Binding{2, counterBuffer.genBinding()}
      }
  );
  vk::DescriptorSet simVkSet = simSet.getVkSet();
  cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                            simulatePipeline.getVkPipelineLayout(),
                            0, 1, &simVkSet, 0, nullptr);

  struct SimPush {
      float dt;
  } pushParams_sim { 0.017f };

  cmd_buf.pushConstants(
      simulatePipeline.getVkPipelineLayout(),
      vk::ShaderStageFlagBits::eCompute,
      0,
      sizeof(pushParams_sim),
      &pushParams_sim
  );

  uint32_t workgroupSize_sim = 256;
  uint32_t numGroups_sim = (maxParticles + workgroupSize_sim - 1) / workgroupSize_sim;
  cmd_buf.dispatch(numGroups_sim, 1, 1);

  // WRITE INDIRECT
  cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, writeIndirectPipeline.getVkPipeline());
  auto writeIndirectInfo = etna::get_shader_program("writeIndirect");
  auto writeIndirectSet = etna::create_descriptor_set(
      writeIndirectInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
          etna::Binding{2, counterBuffer.genBinding()},
          etna::Binding{3, indirectBuffer.genBinding()}
      }
  );
  vk::DescriptorSet writeIndirectVkSet = writeIndirectSet.getVkSet();
  cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                            writeIndirectPipeline.getVkPipelineLayout(),
                            0, 1, &writeIndirectVkSet, 0, nullptr);

  struct IndirectPush {
      uint32_t vertsPerParticle;
  } pushParams_indir { 6 };

  cmd_buf.pushConstants(
      writeIndirectPipeline.getVkPipelineLayout(),
      vk::ShaderStageFlagBits::eCompute,
      0,
      sizeof(pushParams_indir),
      &pushParams_indir
  );
                            
  uint32_t workgroupSize_indir = 1;
  uint32_t numGroups_indir = (maxParticles + workgroupSize_indir - 1) / workgroupSize_indir;
  cmd_buf.dispatch(numGroups_indir, 1, 1);


  ///
  ///
  /// GRAPHICS PART
  ///
  ///


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

    auto particles2Info = etna::get_shader_program("particles2");
    auto set = etna::create_descriptor_set(
      particles2Info.getDescriptorLayoutId(0),
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

    uniformParams.invViewProj = glm::inverse(worldViewProj);

    struct Params {
      glm::vec4 cameraPos; glm::vec4 lightPos; glm::uvec2 res; glm::uvec2 mouse; float time; float planetSpeed;
    } params{glm::vec4(cameraPos, 1), glm::vec4(lightPos, 1), resolution, mouse, time, planetSpeed};

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
            etna::Binding{ 0, constants.genBinding() },

            etna::Binding{ 1, particleBuffer.genBinding() }
        }
    );

    vk::DescriptorSet emitterVkSet = emitterSet.getVkSet();
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               emittersPipeline.getVkPipelineLayout(), 0, 1, &emitterVkSet, 0, nullptr);

    struct PushConsts {
        glm::vec4 color;
        glm::vec3 pos;
        float size;
        float alpha;
    };

    uniformParams.viewProj = worldViewProj;
    uniformParams.view = view;
    uniformParams.camPos = glm::vec4(cameraPos, 1);
/*
    emitterRenderOrder.resize(emitters.size());
    std::iota(emitterRenderOrder.begin(), emitterRenderOrder.end(), 0);
    std::sort(emitterRenderOrder.begin(), emitterRenderOrder.end(),
        [&](size_t a, size_t b) {
            float da = glm::distance(emitters[a].position, cameraPos);
            float db = glm::distance(emitters[b].position, cameraPos);
            return da > db;
    });

    for (size_t idx : emitterRenderOrder) {
      auto& emitter = emitters[idx];
      for (auto& p : emitter.particleList) {
        PushConsts pc{glm::vec4(emitter.particleColor, 1), p.pos, emitter.particleSize, 1.0f - (p.age / p.lifetime)};
        cmd_buf.pushConstants(emittersPipeline.getVkPipelineLayout(),
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                              0, sizeof(PushConsts), &pc);
        cmd_buf.draw(6, 1, 0, 0);
      }
    }
*/
    // малюємо всіх частинок GPU, як підказав writeIndirect
    cmd_buf.drawIndirect(indirectBuffer.get(), 0, 1, sizeof(VkDrawIndirectCommand));

  }
}

void WorldRenderer::drawGui()
{
  ImGui::Begin("Simple render settings");

  ImGui::SliderFloat("Planet speed", &planetSpeed, 0.0f, 20.0f);

  ImGui::SliderFloat("Surface texture scale", &scale, 0.f, 300.0f);

  float spaceColor[3]{uniformParams.spaceColor.r, uniformParams.spaceColor.g, uniformParams.spaceColor.b};
  ImGui::ColorEdit3(
    "Cubemap Space Color", spaceColor, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
  uniformParams.spaceColor = {spaceColor[0], spaceColor[1], spaceColor[2]};

  float waveColor[3]{uniformParams.waveColor.r, uniformParams.waveColor.g, uniformParams.waveColor.b};
  ImGui::ColorEdit3(
    "Cubemap Wave Color", waveColor, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
  uniformParams.waveColor = {waveColor[0], waveColor[1], waveColor[2]};

  if (ImGui::Button("Default values")) {
    planetSpeed = 1.0f;
    scale = 20.0f;
    uniformParams.spaceColor = {0.0f, 0.0f, 0.0f};
    uniformParams.waveColor = {0.15f, 0.75f, 0.03f};
  }

  ImGui::SliderFloat3("Light Position", &lightPos.x, -200.f, 200.f);

  if (ImGui::CollapsingHeader("Emitters")) {
    if (ImGui::Button("Add Emitter")) {
        emitters.push_back(Emitter{
            .position = {0.0f, 0.0f, 0.0f},
            .spawnRate = 1500.0f,
            .particleLifetime = 1.0f,
            .initialSpeed = 1.0f,
            .particleSize = 0.01f,
            .particleColor = {0.0f, 0.0f, 1.0f},
            .particleList = {}
        });
    }

    for (size_t i = 0; i < emitters.size(); ++i) {
      ImGui::PushID((int)i);

      if (ImGui::TreeNode(("Emitter " + std::to_string(i)).c_str())) {            
        float pos[3] = {emitters[i].position.x, emitters[i].position.y, emitters[i].position.z};
        ImGui::SliderFloat3("Position", pos, -10.f, 10.f);
        emitters[i].position = {pos[0], pos[1], pos[2]};

        ImGui::SliderFloat("Spawn rate", &emitters[i].spawnRate, 10.f, 10000.f);
        ImGui::SliderFloat("Lifetime", &emitters[i].particleLifetime, 0.1f, 10.f);
        ImGui::SliderFloat("Initial speed", &emitters[i].initialSpeed, 0.f, 10.f);
        ImGui::SliderFloat("Size", &emitters[i].particleSize, 0.f, 0.1f);

        float color[3] = {emitters[i].particleColor.r, emitters[i].particleColor.g, emitters[i].particleColor.b};
        ImGui::ColorEdit3("Particle Color", color);
        emitters[i].particleColor = {color[0], color[1], color[2]};

        if (ImGui::Button("Remove Emitter")) {
            emitters.erase(emitters.begin() + i);
            ImGui::TreePop();
            ImGui::PopID();
            break;
        }
        ImGui::TreePop();
      }
      ImGui::PopID();
    }
  }

  ImGui::Text(
    "Application average %.3f ms/frame (%.1f FPS)",
    1000.0f / ImGui::GetIO().Framerate,
    ImGui::GetIO().Framerate);

  ImGui::NewLine();

  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Press 'B' to recompile and reload shaders");
  ImGui::End();
}
/*
void WorldRenderer::spawnParticles(Emitter& emitter, float deltaTime) {
    int count = static_cast<int>(emitter.spawnRate * deltaTime);

    for (int i = 0; i < count; i++) {
        Particle p;
        p.pos = emitter.position;
        p.vel = glm::sphericalRand(1.0f) * emitter.initialSpeed;
        p.lifetime = emitter.particleLifetime;
        p.age = 0.0f;

        emitter.particleList.push_back(p);
    }
}
*/
