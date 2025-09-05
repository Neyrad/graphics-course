
add_executable(particles
  main.cpp
  Renderer.cpp
  WorldRenderer.cpp
  App.cpp
)

target_link_libraries(particles
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(particles
  shaders/toy.vert
  shaders/toy.frag
  shaders/texture.frag
)