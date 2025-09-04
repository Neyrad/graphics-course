#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <glm/glm.hpp>

#include "scene/SceneManager.hpp"
#include "render_utils/QuadRenderer.hpp"
#include "wsi/Keyboard.hpp"
#include "FramePacket.hpp"

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


  glm::mat4x4 worldViewProj;

  float time;
  glm::vec2 mouse;
  float yaw;
  float pitch;

  float planetSpeed;

  etna::Image image;
  etna::Sampler textureSampler;
  etna::Image texture;

  etna::GraphicsPipeline texturePipeline{};
  etna::GraphicsPipeline graphicsPipeline{};

  glm::uvec2 resolution;
};
