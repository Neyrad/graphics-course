
add_executable(imgui
  main.cpp
  Renderer.cpp
  WorldRenderer.cpp
  App.cpp
)

target_link_libraries(imgui
  PRIVATE glfw etna glm::glm wsi gui scene)

target_add_shaders(imgui
  shaders/toy.vert
  shaders/toy.frag
  shaders/texture.frag
)