#include "WorldRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <etna/Buffer.hpp>
#include <glm/ext.hpp>
#include <imgui.h>
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <numeric>
#include <cstdlib>
#include <ctime> 

#include "stb_image.h"

const uint32_t maxParticles = 10000;

WorldRenderer::WorldRenderer()
  : sceneMgr{std::make_unique<SceneManager>()}
{
  lightPos = glm::vec3(3.0f, 11.0f, 9.0f);
  std::srand(std::time(nullptr));
}

void WorldRenderer::allocateResources(glm::uvec2 swapchain_resolution)
{
  resolution = swapchain_resolution;

  auto& ctx = etna::get_context();

  constants = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "constants",
  });

  constants.map();

  shadowMap = ctx.createImage(etna::Image::CreateInfo{
      .extent = vk::Extent3D{1024, 1024, 1},
      .name = "shadowMap",
      .format = vk::Format::eD32Sfloat,
      .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | 
                    vk::ImageUsageFlagBits::eSampled,
  });

  shadowSampler = etna::Sampler{etna::Sampler::CreateInfo{
    .filter = vk::Filter::eLinear,
    .addressMode = vk::SamplerAddressMode::eClampToEdge,
    .name = "shadowSampler"
  }};

  mainViewDepth = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "main_view_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
  });

  vertices.clear();

  vertices.emplace_back(Vertex{glm::vec4(-1,-1,-1,1), glm::vec4(0,0,-1,0)});
  vertices.emplace_back(Vertex{glm::vec4( 1,-1,-1,1), glm::vec4(0,0,-1,0)});
  vertices.emplace_back(Vertex{glm::vec4( 1, 1,-1,1), glm::vec4(0,0,-1,0)});
  vertices.emplace_back(Vertex{glm::vec4(-1,-1,-1,1), glm::vec4(0,0,-1,0)});
  vertices.emplace_back(Vertex{glm::vec4( 1, 1,-1,1), glm::vec4(0,0,-1,0)});
  vertices.emplace_back(Vertex{glm::vec4(-1, 1,-1,1), glm::vec4(0,0,-1,0)});

  vertices.emplace_back(Vertex{glm::vec4(-1,-1, 1,1), glm::vec4(0,0,1,0)});
  vertices.emplace_back(Vertex{glm::vec4( 1,-1, 1,1), glm::vec4(0,0,1,0)});
  vertices.emplace_back(Vertex{glm::vec4( 1, 1, 1,1), glm::vec4(0,0,1,0)});
  vertices.emplace_back(Vertex{glm::vec4(-1,-1, 1,1), glm::vec4(0,0,1,0)});
  vertices.emplace_back(Vertex{glm::vec4( 1, 1, 1,1), glm::vec4(0,0,1,0)});
  vertices.emplace_back(Vertex{glm::vec4(-1, 1, 1,1), glm::vec4(0,0,1,0)});

  vertices.emplace_back(Vertex{glm::vec4(-1,-1,-1,1), glm::vec4(-1,0,0,0)});
  vertices.emplace_back(Vertex{glm::vec4(-1,-1, 1,1), glm::vec4(-1,0,0,0)});
  vertices.emplace_back(Vertex{glm::vec4(-1, 1, 1,1), glm::vec4(-1,0,0,0)});
  vertices.emplace_back(Vertex{glm::vec4(-1,-1,-1,1), glm::vec4(-1,0,0,0)});
  vertices.emplace_back(Vertex{glm::vec4(-1, 1, 1,1), glm::vec4(-1,0,0,0)});
  vertices.emplace_back(Vertex{glm::vec4(-1, 1,-1,1), glm::vec4(-1,0,0,0)});

  vertices.emplace_back(Vertex{glm::vec4(1,-1,-1,1), glm::vec4(1,0,0,0)});
  vertices.emplace_back(Vertex{glm::vec4(1,-1, 1,1), glm::vec4(1,0,0,0)});
  vertices.emplace_back(Vertex{glm::vec4(1, 1, 1,1), glm::vec4(1,0,0,0)});
  vertices.emplace_back(Vertex{glm::vec4(1,-1,-1,1), glm::vec4(1,0,0,0)});
  vertices.emplace_back(Vertex{glm::vec4(1, 1, 1,1), glm::vec4(1,0,0,0)});
  vertices.emplace_back(Vertex{glm::vec4(1, 1,-1,1), glm::vec4(1,0,0,0)});

  vertices.emplace_back(Vertex{glm::vec4(-1,-1,-1,1), glm::vec4(0,-1,0,0)});
  vertices.emplace_back(Vertex{glm::vec4( 1,-1,-1,1), glm::vec4(0,-1,0,0)});
  vertices.emplace_back(Vertex{glm::vec4( 1,-1, 1,1), glm::vec4(0,-1,0,0)});
  vertices.emplace_back(Vertex{glm::vec4(-1,-1,-1,1), glm::vec4(0,-1,0,0)});
  vertices.emplace_back(Vertex{glm::vec4( 1,-1, 1,1), glm::vec4(0,-1,0,0)});
  vertices.emplace_back(Vertex{glm::vec4(-1,-1, 1,1), glm::vec4(0,-1,0,0)});

  vertices.emplace_back(Vertex{glm::vec4(-1, 1,-1,1), glm::vec4(0,1,0,0)});
  vertices.emplace_back(Vertex{glm::vec4( 1, 1,-1,1), glm::vec4(0,1,0,0)});
  vertices.emplace_back(Vertex{glm::vec4( 1, 1, 1,1), glm::vec4(0,1,0,0)});
  vertices.emplace_back(Vertex{glm::vec4(-1, 1,-1,1), glm::vec4(0,1,0,0)});
  vertices.emplace_back(Vertex{glm::vec4( 1, 1, 1,1), glm::vec4(0,1,0,0)});
  vertices.emplace_back(Vertex{glm::vec4(-1, 1, 1,1), glm::vec4(0,1,0,0)});

  vertexBuffer = ctx.createBuffer(etna::Buffer::CreateInfo{
      .size = sizeof(Vertex) * vertices.size(),
      .bufferUsage = vk::BufferUsageFlagBits::eVertexBuffer,
      .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
      .name = "vertexBuffer"
  });

  vertexBuffer.map();
  std::memcpy(vertexBuffer.data(), vertices.data(), sizeof(Vertex) * vertices.size());

  glm::mat4x4 floor = glm::scale(glm::mat4x4(1.0f), glm::vec3(10.0f, 1.0f, 10.0f));
  floor = glm::translate(floor, glm::vec3(0, -2, 0));
  models.push_back(floor);

  glm::mat4x4 ceilingCube = glm::translate(glm::mat4x4(1.0f), glm::vec3(0, 18.0f, 0));
  ceilingCube = glm::scale(ceilingCube, glm::vec3(10.0f, 1.0f, 10.0f));

  glm::mat4x4 wallX = glm::rotate(glm::mat4x4(1.0f), glm::radians(90.0f), glm::vec3(1,0,0));
  glm::mat4x4 wallZ = glm::rotate(glm::mat4x4(1.0f), glm::radians(90.0f), glm::vec3(0,0,1));

  glm::mat4x4 wallLeft   = glm::translate(wallZ, glm::vec3(9.0f, 11.0f, 0.0f));
  glm::mat4x4 wallRight  = glm::translate(wallZ, glm::vec3(9.0f, -11.0f, 0.0f));
  glm::mat4x4 wallFront  = glm::translate(wallX, glm::vec3(0.0f, 11.0f, -9.0f));
  glm::mat4x4 wallBack   = glm::translate(wallX, glm::vec3(0.0f, -11.0f, -9.0f));

  wallLeft = glm::scale(wallLeft, glm::vec3(10.0f, 1.0f, 10.0f));
  wallRight = glm::scale(wallRight, glm::vec3(10.0f, 1.0f, 10.0f));
  wallFront = glm::scale(wallFront, glm::vec3(10.0f, 1.0f, 10.0f));
  wallBack = glm::scale(wallBack, glm::vec3(10.0f, 1.0f, 10.0f)); 

  models.push_back(ceilingCube);
  models.push_back(wallLeft);
  models.push_back(wallRight);
  models.push_back(wallFront);
  models.push_back(wallBack);

  glm::mat4x4 back = glm::rotate(glm::mat4x4(1.0f), glm::radians(90.0f), glm::vec3(1,0,0));

  glm::mat4x4 seat = glm::translate(glm::mat4x4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f));
  back = glm::translate(back, glm::vec3(0.0f, -0.5f, -2.0f));

  seat = glm::scale(seat, glm::vec3(1.0f, 0.1f, 1.0f));
  back = glm::scale(back, glm::vec3(1.0f, 0.1f, 0.5f));

  models.push_back(seat);
  models.push_back(back);

  glm::mat4x4 back1 = glm::translate(glm::mat4x4(1.0f), glm::vec3(0.7f, 1.0f, -0.7f));
  glm::mat4x4 back2 = glm::translate(glm::mat4x4(1.0f), glm::vec3(-0.7f, 1.0f, -0.7f));

  back1 = glm::scale(back1, glm::vec3(0.15f, 1.2f, 0.15f));
  back2 = glm::scale(back2, glm::vec3(0.15f, 1.2f, 0.15f));

  models.push_back(back1);
  models.push_back(back2);

  glm::mat4x4 leg1 = glm::translate(glm::mat4x4(1.0f), glm::vec3(0.7f, -0.3f, 0.7f));
  glm::mat4x4 leg2 = glm::translate(glm::mat4x4(1.0f), glm::vec3(-0.7f, -0.3f, 0.7f));
  glm::mat4x4 leg3 = glm::translate(glm::mat4x4(1.0f), glm::vec3(0.7f, -0.3f, -0.7f));
  glm::mat4x4 leg4 = glm::translate(glm::mat4x4(1.0f), glm::vec3(-0.7f, -0.3f, -0.7f));

  leg1 = glm::scale(leg1, glm::vec3(0.15f, 0.7f, 0.15f));
  leg2 = glm::scale(leg2, glm::vec3(0.15f, 0.7f, 0.15f));
  leg3 = glm::scale(leg3, glm::vec3(0.15f, 0.7f, 0.15f));
  leg4 = glm::scale(leg4, glm::vec3(0.15f, 0.7f, 0.15f));

  models.push_back(leg1);
  models.push_back(leg2);
  models.push_back(leg3);
  models.push_back(leg4);

  imageHalfRes = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x / 2, resolution.y / 2, 1},
    .name = "half_res_fog",
    .format = vk::Format::eR16G16B16A16Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment });
}

void WorldRenderer::loadShaders()
{
  etna::create_program(
    "fog",
    {FOG_SHADERS_ROOT "fog.frag.spv", FOG_SHADERS_ROOT "fog.vert.spv"});

  etna::create_program(
    "emitters",
    {FOG_SHADERS_ROOT "particles.frag.spv",
    FOG_SHADERS_ROOT "particles.vert.spv"});

  etna::create_program(
    "simulate",
    { FOG_SHADERS_ROOT "simulate.comp.spv" }
  );

  etna::create_program(
    "spawn",
    { FOG_SHADERS_ROOT "spawn.comp.spv" }
  );

  etna::create_program(
    "writeIndirect",
    { FOG_SHADERS_ROOT "writeIndirect.comp.spv" }
  );

  etna::create_program(
    "sort",
    { FOG_SHADERS_ROOT "sort.comp.spv" }
  );

  etna::create_program(
    "shadow",
    {FOG_SHADERS_ROOT "shadow.frag.spv",
    FOG_SHADERS_ROOT "shadow.vert.spv"});

  etna::create_program(
    "fog_half_res",
    {FOG_SHADERS_ROOT "halfFog.frag.spv",
    FOG_SHADERS_ROOT "halfFog.vert.spv"});
}

void WorldRenderer::setupPipelines(vk::Format swapchain_format)
{
  graphicsPipeline = etna::get_context().getPipelineManager().createGraphicsPipeline(
    "fog",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput =
      {
        .colorAttachmentFormats = {swapchain_format},
        .depthAttachmentFormat = vk::Format::eD32Sfloat,
      }
    }
  );

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

  sortPipeline = etna::get_context().getPipelineManager().createComputePipeline(
      "sort",
      etna::ComputePipeline::CreateInfo{}
  );

  shadowPipeline = etna::get_context().getPipelineManager().createGraphicsPipeline(
      "shadow",
      etna::GraphicsPipeline::CreateInfo{
          .fragmentShaderOutput = {
              .depthAttachmentFormat = vk::Format::eD32Sfloat
          }
      }
  );

  fogPipeline = etna::get_context().getPipelineManager().createGraphicsPipeline(
    "fog_half_res",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput =
      {
        .colorAttachmentFormats = {swapchain_format},
        .depthAttachmentFormat = vk::Format::eD32Sfloat,
      }
    }
  );
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

  this->deltaTime = FP.time - this->time;
  this->time = FP.time;
  this->mouse = FP.mouse;

  uniformParams.viewProj = worldViewProj;
  uniformParams.invViewProj = glm::inverse(worldViewProj);
  uniformParams.view = view;
  uniformParams.camPos = glm::vec4(cameraPos, 1);

  glm::mat4x4 lightView = glm::lookAt(
      glm::vec3(lightPos),
      glm::vec3(0.0f),
      glm::vec3(0, 1, 0)
  );

  farPlane = abs(lightPos.x) + abs(lightPos.y) + abs(lightPos.z);

  glm::mat4x4 lightProj = glm::ortho(
      -halfSize, halfSize,
      -halfSize, halfSize,
      nearPlane, farPlane
  );

  uniformParams.lightVP = lightProj * lightView;
  uniformParams.nearPlane = nearPlane;
  uniformParams.farPlane = farPlane;

  uniformParams.time = time;

  std::memcpy(constants.data(), &uniformParams, sizeof(uniformParams));
}

void WorldRenderer::renderWorld(vk::CommandBuffer cmd_buf,
                                vk::Image target_image, vk::ImageView target_image_view)
{
  ///
  ///
  /// COMPUTE PART
  ///
  ///

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

    // RESET OUTBUFFER ALIVE COUNTER
    cmd_buf.fillBuffer((emitter.useAasInput ? emitter.counterBufferB : emitter.counterBufferA).get(), 0, sizeof(uint32_t), 0);
    
    vk::BufferMemoryBarrier2 resetBarrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
        .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
        .dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
        .buffer = (emitter.useAasInput ? emitter.counterBufferB : emitter.counterBufferA).get(),
        .offset = 0,
        .size = sizeof(uint32_t)
    };
    cmd_buf.pipelineBarrier2(vk::DependencyInfo{}.setBufferMemoryBarriers(resetBarrier));

    // SPAWN
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, spawnPipeline.getVkPipeline());
    auto spawnInfo = etna::get_shader_program("spawn");
    auto spawnSet = etna::create_descriptor_set(
        spawnInfo.getDescriptorLayoutId(0),
        cmd_buf,
        {
            etna::Binding{1, (emitter.useAasInput ? emitter.particleBufferB : emitter.particleBufferA).genBinding()},
            etna::Binding{3, (emitter.useAasInput ? emitter.counterBufferB : emitter.counterBufferA).genBinding()} 
        }
    );
    vk::DescriptorSet spawnVkSet = spawnSet.getVkSet();
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                              spawnPipeline.getVkPipelineLayout(),
                              0, 1, &spawnVkSet, 0, nullptr);

    struct SpawnPush {
        glm::vec4 emitterPos;
        glm::vec4 color;
        uint32_t spawnCount;
        float life;
        uint32_t seed;
        float size;
        float initialSpeed;
    };

    SpawnPush pushParams_spawn {
        glm::vec4(emitter.position, 1.0f),
        glm::vec4(emitter.particleColor, 1.0f),
        static_cast<uint32_t>(emitter.spawnRate * deltaTime),
        emitter.particleLifetime,
        static_cast<uint32_t>(std::rand()),
        emitter.particleSize,
        emitter.initialSpeed
    };

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

    cmd_buf.pipelineBarrier2(vk::DependencyInfo{}.setBufferMemoryBarriers(resetBarrier));

    // SIMULATE
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, simulatePipeline.getVkPipeline());
    auto simInfo = etna::get_shader_program("simulate");
    auto simSet = etna::create_descriptor_set(
      simInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
          etna::Binding{0, (emitter.useAasInput ? emitter.particleBufferA : emitter.particleBufferB).genBinding()},
          etna::Binding{1, (emitter.useAasInput ? emitter.particleBufferB : emitter.particleBufferA).genBinding()},
          etna::Binding{2, (emitter.useAasInput ? emitter.counterBufferA : emitter.counterBufferB).genBinding()},
          etna::Binding{3, (emitter.useAasInput ? emitter.counterBufferB : emitter.counterBufferA).genBinding()}
      }
    );
    vk::DescriptorSet simVkSet = simSet.getVkSet();
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                              simulatePipeline.getVkPipelineLayout(),
                              0, 1, &simVkSet, 0, nullptr);

    struct SimPush {
        float dt;
    } pushParams_sim { deltaTime };

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

    vk::BufferMemoryBarrier2 counterBarrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
        .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
        .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
        .buffer = (emitter.useAasInput ? emitter.counterBufferB : emitter.counterBufferA).get(),
        .offset = 0,
        .size = sizeof(uint32_t)
    };
    cmd_buf.pipelineBarrier2(vk::DependencyInfo{}.setBufferMemoryBarriers(counterBarrier));

    // SORT
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, sortPipeline.getVkPipeline());
    auto sortInfo = etna::get_shader_program("sort");
    auto sortSet = etna::create_descriptor_set(
      sortInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
          etna::Binding{1, (emitter.useAasInput ? emitter.particleBufferB : emitter.particleBufferA).genBinding()},
          etna::Binding{3, (emitter.useAasInput ? emitter.counterBufferB : emitter.counterBufferA).genBinding()},
          etna::Binding{5, emitter.indicesBuffer.genBinding()},
          etna::Binding{6, emitter.depthBuffer.genBinding()},
      }
    );
    vk::DescriptorSet sortVkSet = sortSet.getVkSet();
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                              sortPipeline.getVkPipelineLayout(),
                              0, 1, &sortVkSet, 0, nullptr);

    struct SortPush { glm::vec3 camPos; } push { cameraPos };
    cmd_buf.pushConstants(sortPipeline.getVkPipelineLayout(),
                          vk::ShaderStageFlagBits::eCompute,
                          0, sizeof(push), &push);

    uint32_t workgroupSize = 256;
    uint32_t numGroups = (maxParticles + workgroupSize - 1) / workgroupSize;
    cmd_buf.dispatch(numGroups, 1, 1);

    vk::BufferMemoryBarrier2 barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
        .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eVertexShader,
        .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
        .buffer = emitter.indicesBuffer.get(),
        .offset = 0,
        .size = sizeof(uint32_t) * maxParticles
    };
    cmd_buf.pipelineBarrier2(vk::DependencyInfo{}.setBufferMemoryBarriers(barrier));

    cmd_buf.pipelineBarrier2(vk::DependencyInfo{}.setBufferMemoryBarriers(counterBarrier));


    // WRITE INDIRECT
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, writeIndirectPipeline.getVkPipeline());
    auto writeIndirectInfo = etna::get_shader_program("writeIndirect");
    auto writeIndirectSet = etna::create_descriptor_set(
        writeIndirectInfo.getDescriptorLayoutId(0),
        cmd_buf,
        {
            etna::Binding{3, (emitter.useAasInput ? emitter.counterBufferB : emitter.counterBufferA).genBinding()},
            etna::Binding{4, emitter.indirectBuffer.genBinding()}
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
                              
    cmd_buf.dispatch(1, 1, 1);
    vk::BufferMemoryBarrier2 indirectBarrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
        .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eDrawIndirect,
        .dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead,
        .buffer = emitter.indirectBuffer.get(),
        .offset = 0,
        .size = sizeof(VkDrawIndirectCommand)
    };
    cmd_buf.pipelineBarrier2(vk::DependencyInfo{}.setBufferMemoryBarriers(indirectBarrier));
  }

  ///
  ///
  /// GRAPHICS PART
  ///
  ///

  // --- PASS 0: shadow map ---
  {
    etna::set_state(cmd_buf, shadowMap.get(),
      vk::PipelineStageFlagBits2::eEarlyFragmentTests,
      vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      vk::ImageLayout::eDepthAttachmentOptimal,
      vk::ImageAspectFlagBits::eDepth);

    etna::flush_barriers(cmd_buf);

    etna::RenderTargetState shadowRT(
      cmd_buf,
      {{0,0}, {1024, 1024}},
      {},
      { .image = shadowMap.get(), .view = shadowMap.getView({}) }
    );

    auto shadowInfo = etna::get_shader_program("shadow");

    auto set = etna::create_descriptor_set(
      shadowInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
        // binding 1 -> uniform buffer
        etna::Binding{ 1, constants.genBinding() },

        // binding 2 -> vertex buffer
        etna::Binding{ 2, vertexBuffer.genBinding() },
      });

    vk::DescriptorSet vkSet = set.getVkSet();

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, shadowPipeline.getVkPipeline());

    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               shadowPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);


    for (uint32_t i = 0; i < models.size(); ++i) {
      if (i < 6) continue;

      struct Params {
        glm::mat4x4 model;
      } params { models[i] };

      cmd_buf.pushConstants(shadowPipeline.getVkPipelineLayout(),
                            vk::ShaderStageFlagBits::eVertex, 0, sizeof(params), &params);
      cmd_buf.draw(vertices.size(), 1, 0, 0);
    }
  }
    // --- PASS 1: fog ---

    etna::set_state(cmd_buf, imageHalfRes.get(),
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::ImageAspectFlagBits::eColor);

    etna::flush_barriers(cmd_buf);

    {
      etna::RenderTargetState rtFog(
        cmd_buf,
        {{0, 0}, {resolution.x / 2, resolution.y / 2}},
        {{ .image = imageHalfRes.get(), .view = imageHalfRes.getView({}) }},
        {}
      );

      auto fogInfo = etna::get_shader_program("fog_half_res");
      auto set = etna::create_descriptor_set(
        fogInfo.getDescriptorLayoutId(0),
        cmd_buf,
        {
          etna::Binding{ 2, constants.genBinding() },
          etna::Binding{ 3, shadowMap.genBinding(shadowSampler.get(),
                            vk::ImageLayout::eShaderReadOnlyOptimal) },
        });

      vk::DescriptorSet vkSet = set.getVkSet();
      cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, fogPipeline.getVkPipeline());
      cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                fogPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);

      struct Params {
        glm::vec4 lightPos;
        glm::vec4 fogColor;
        float minFogDensity;
        float maxFogDensity;
        float baseLightLevel;
        float fogSpeed;
        uint32_t enableFog;
        uint32_t numSteps;
      };

      Params params {
        glm::vec4(lightPos, 1.0f),
        glm::vec4(fogColor, 1.0f),
        minFogDensity,
        maxFogDensity,
        baseLightLevel,
        fogSpeed,
        enableFog ? 1u : 0u,
        numSteps
      };

      cmd_buf.pushConstants(fogPipeline.getVkPipelineLayout(),
                            vk::ShaderStageFlagBits::eFragment, 0, sizeof(params), &params);
      cmd_buf.draw(3, 1, 0, 0);
    }

 
  // --- PASS 2: render to swapchain, sample 'image' ---
  {
    etna::set_state(cmd_buf, target_image,
                  vk::PipelineStageFlagBits2::eFragmentShader,
                  vk::AccessFlagBits2::eShaderRead,
                  vk::ImageLayout::eShaderReadOnlyOptimal,
                  vk::ImageAspectFlagBits::eColor);
  
    etna::flush_barriers(cmd_buf);

    etna::RenderTargetState rt2(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{ .image = target_image, .view = target_image_view }},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({})} );

    auto fogInfo = etna::get_shader_program("fog");
    auto set = etna::create_descriptor_set(
      fogInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
        // binding 2 -> uniform buffer
        etna::Binding{ 2, constants.genBinding() },

        // binding 3 -> shadow map
        etna::Binding{ 3, shadowMap.genBinding(shadowSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) },

        // binding 4 -> vertex buffer
        etna::Binding{ 4, vertexBuffer.genBinding() },

        // binding 5 -> for half res
        etna::Binding{ 5, imageHalfRes.genBinding(shadowSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) }
      });

    vk::DescriptorSet vkSet = set.getVkSet();
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               graphicsPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);

    for (uint32_t i = 0; i < models.size(); ++i) {
      struct Params {
        glm::mat4x4 model;
        glm::vec4 lightPos;
        uint32_t id;
      } params { models[i], glm::vec4(lightPos, 1), i };

      cmd_buf.pushConstants(graphicsPipeline.getVkPipelineLayout(),
                            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(params), &params);
      cmd_buf.draw(vertices.size(), 1, 0, 0);
    }

    for (size_t idx : emitterRenderOrder) {
      auto& emitter = emitters[idx];
      // --- PARTICLES ---
      cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, emittersPipeline.getVkPipeline());

      auto emittersInfo = etna::get_shader_program("emitters");
      auto emitterSet = etna::create_descriptor_set(
          emittersInfo.getDescriptorLayoutId(0),
          cmd_buf,
          {
              etna::Binding{ 0, constants.genBinding() },
              etna::Binding{ 1, (emitter.useAasInput ? emitter.particleBufferB : emitter.particleBufferA).genBinding() },
              etna::Binding{ 5, emitter.indicesBuffer.genBinding() },
          }
      );

      vk::DescriptorSet emitterVkSet = emitterSet.getVkSet();
      cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                emittersPipeline.getVkPipelineLayout(), 0, 1, &emitterVkSet, 0, nullptr);

      cmd_buf.drawIndirect(emitter.indirectBuffer.get(), 0, 1, sizeof(VkDrawIndirectCommand));
      emitter.useAasInput = !emitter.useAasInput;
    }
  }
}

void WorldRenderer::drawGui()
{
  ImGui::Begin("Simple render settings");
  ImGui::Checkbox("Fog", &enableFog);

  ImGui::SliderFloat("minFogDensity", &minFogDensity, 0.0f, 5.0f);
  ImGui::SliderFloat("maxFogDensity", &maxFogDensity, 0.0f, 5.0f);
  ImGui::SliderFloat("baseLightLevel", &baseLightLevel, -0.2f, 0.2f);

  ImGui::SliderFloat("fogSpeed", &fogSpeed, -10.0f, 10.0f);

  float color[3] = {fogColor.r, fogColor.g, fogColor.b};
  ImGui::ColorEdit3("Fog Color", color);
  fogColor = {color[0], color[1], color[2]};

  float light[3]{lightPos.x, lightPos.y, lightPos.z};
  ImGui::Text("Light Position");
  ImGui::SliderFloat("X", &light[0], -9.0f, 9.0f);
  ImGui::SliderFloat("Y", &light[1], 11.0f, 16.0f);
  ImGui::SliderFloat("Z", &light[2], -9.0f, 9.0f);
  lightPos = {light[0], light[1], light[2]};

  ImGui::Text("Quality");
  const char* items[] = { "8", "16", "32", "64", "128", "256", "512", "1024" };

  if (ImGui::BeginCombo("Num Steps", items[pickStepsQuality])) {
      for (uint32_t n = 0; n < IM_ARRAYSIZE(items); n++) {
          bool isSelected = (pickStepsQuality == n);
          if (ImGui::Selectable(items[n], isSelected))
              pickStepsQuality = n;
          if (isSelected)
              ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
  }

  switch (pickStepsQuality) {
    case 0: numSteps = 8; break;
    case 1: numSteps = 16; break;
    case 2: numSteps = 32; break;
    case 3: numSteps = 64; break;
    case 4: numSteps = 128; break;
    case 5: numSteps = 256; break;
    case 6: numSteps = 512; break;
    case 7: numSteps = 1024; break;
  }

  if (ImGui::CollapsingHeader("Emitters")) {
    if (ImGui::Button("Add Emitter")) {
        Emitter emitter{
            .position = {0.0f, 0.0f, 0.0f},
            .spawnRate = 1500.0f,
            .particleLifetime = 1.0f,
            .initialSpeed = 1.0f,
            .particleSize = 0.01f,
            .particleColor = {0.0f, 0.0f, 1.0f},
            .useAasInput = true,
            .particleBufferA = {},
            .particleBufferB = {},
            .counterBufferA = {},
            .counterBufferB = {},
            .indirectBuffer = {},
            .indicesBuffer = {},
            .depthBuffer = {},
            .followLight = false
        };

        auto& ctx = etna::get_context();
        emitter.particleBufferA = ctx.createBuffer({sizeof(Particle) * maxParticles,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer,
            VMA_MEMORY_USAGE_GPU_ONLY, "emitter_particleBufferA"});
        emitter.particleBufferB = ctx.createBuffer({sizeof(Particle) * maxParticles,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer,
            VMA_MEMORY_USAGE_GPU_ONLY, "emitter_particleBufferB"});
        emitter.counterBufferA = ctx.createBuffer({sizeof(uint32_t),
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_MEMORY_USAGE_GPU_ONLY, "emitter_counterBufferA"});
        emitter.counterBufferB = ctx.createBuffer({sizeof(uint32_t),
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_MEMORY_USAGE_GPU_ONLY, "emitter_counterBufferB"});
        emitter.indirectBuffer = ctx.createBuffer({sizeof(VkDrawIndirectCommand),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
            VMA_MEMORY_USAGE_CPU_TO_GPU, "emitter_indirectBuffer"});
        emitter.indicesBuffer = ctx.createBuffer({sizeof(uint32_t) * maxParticles,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer,
            VMA_MEMORY_USAGE_GPU_ONLY, "emitter_indicesBuffer"});
        emitter.depthBuffer = ctx.createBuffer({sizeof(float) * maxParticles,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer,
            VMA_MEMORY_USAGE_GPU_ONLY, "emitter_depthBuffer"});

        emitters.push_back(std::move(emitter));
    }

    for (size_t i = 0; i < emitters.size(); ++i) {
      ImGui::PushID((int)i);

      if (ImGui::TreeNode(("Emitter " + std::to_string(i)).c_str())) {

        ImGui::Checkbox("Follow Light", &emitters[i].followLight);
        if (emitters[i].followLight) {
            emitters[i].position = lightPos;
        }

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