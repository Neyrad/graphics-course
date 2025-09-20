
add_executable(fog
  main.cpp
  Renderer.cpp
  WorldRenderer.cpp
  App.cpp
)

target_link_libraries(fog
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(fog
  shaders/fog.vert
  shaders/fog.frag
  shaders/shadow.vert
  shaders/shadow.frag
  shaders/texture.frag
  shaders/particles.vert
  shaders/particles.frag
  shaders/simulate.comp
  shaders/spawn.comp
  shaders/writeIndirect.comp
  shaders/sort.comp
)