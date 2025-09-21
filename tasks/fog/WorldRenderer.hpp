#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/ComputePipeline.hpp>
#include <glm/glm.hpp>

#include "scene/SceneManager.hpp"
#include "render_utils/QuadRenderer.hpp"
#include "wsi/Keyboard.hpp"
#include "FramePacket.hpp"

#include "shaders/UniformParams.h"

/**
 * The meat of the sample. All things you see on the screen are contained within this class.
 * This what you want to change and expand between different samples.
 */
class WorldRenderer
{
public:
  WorldRenderer();

  void loadScene(std::filesystem::path path);

  void loadShaders();
  void setupPipelines(vk::Format swapchain_format);
  void allocateResources(glm::uvec2 swapchain_resolution);

  void debugInput(const Keyboard& kb);
  void update(FramePacket& FP);
  void drawGui();
  void renderWorld(
    vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view);

private:
  void renderScene(
    vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, vk::PipelineLayout pipeline_layout);

private:
  std::unique_ptr<SceneManager> sceneMgr;

  etna::Buffer constants;

  glm::mat4x4 worldViewProj;
  glm::mat4x4 view;
  glm::vec3 cameraPos;
  glm::vec3 lightPos;

  float time;
  float deltaTime = 0.0f;
  glm::vec2 mouse;

  float planetSpeed = 1.f;
  float scale = 20.f;

  UniformParams uniformParams
  {
    .lightVP = {},
    .viewProj = {},
    .invViewProj = {},
    .view = {},
    .camPos = {},
    .spaceColor = {0.0f, 0.0f, 0.0f},
    .pad1 = {},
    .waveColor = {0.15f, 0.75f, 0.03f},
    .pad2 = {},
  };

  struct Planet {
    float orbitAngle;
    float radius;
  };

  struct Particle {
    glm::vec4 pos;
    glm::vec4 vel;
    glm::vec4 color;
    float lifetime;
    float age;
    float size;
    float pad1;
  };

  struct Emitter {
    glm::vec3 position;
    float spawnRate;
    float particleLifetime;
    float initialSpeed;
    float particleSize;
    glm::vec3 particleColor;

    bool useAasInput;
    etna::Buffer particleBufferA;
    etna::Buffer particleBufferB;

    etna::Buffer counterBufferA;
    etna::Buffer counterBufferB;

    etna::Buffer indirectBuffer;

    // sort
    etna::Buffer indicesBuffer;
    etna::Buffer depthBuffer;
  };

  std::vector<Emitter> emitters;
  std::vector<size_t> emitterRenderOrder;

  std::vector<Planet> planets = {
    {0.3f, 10.f},
    {1.1f, -16.f},
    {2.0f, 12.f},
    {4.5f, -24.f},
    {5.2f, -10.f}
  };

  etna::Image image;
  etna::Sampler textureSampler;
  etna::Image texture;

  etna::GraphicsPipeline texturePipeline{};
  etna::GraphicsPipeline graphicsPipeline{};
  etna::GraphicsPipeline emittersPipeline{};

  etna::ComputePipeline simulatePipeline{};
  etna::ComputePipeline spawnPipeline{};
  etna::ComputePipeline writeIndirectPipeline{};
  etna::ComputePipeline sortPipeline{};

  glm::uvec2 resolution;

  // shadow map
  etna::Image shadowMap;
  etna::Sampler shadowSampler;
  etna::GraphicsPipeline shadowPipeline{};

  etna::Image mainViewDepth;


  etna::Buffer vertexBuffer;

  struct Vertex {
      glm::vec4 pos;
      glm::vec4 normal;
  };
  std::vector<Vertex> vertices;

  std::vector<glm::mat4x4> models;

  float halfSize = 48.0f;
  float nearPlane = 0.0f; // ближня межа, можна трохи більше, щоб включити все
  float farPlane  = 32.0f;  // дальня межа
};
