
add_executable(particles2
  main.cpp
  Renderer.cpp
  WorldRenderer.cpp
  App.cpp
)

target_link_libraries(particles2
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(particles2
  shaders/toy.vert
  shaders/toy.frag
  shaders/texture.frag
  shaders/particles.vert
  shaders/particles.frag
  shaders/simulate.comp
  shaders/spawn.comp
  shaders/writeIndirect.comp
)