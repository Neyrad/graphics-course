include("C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/cmake/CPM_0.40.2.cmake")
CPMAddPackage("NAME;SpirvReflect;GITHUB_REPOSITORY;KhronosGroup/SPIRV-Reflect;GIT_TAG;vulkan-sdk-1.3.290;OPTIONS;SPIRV_REFLECT_EXECUTABLE OFF;SPIRV_REFLECT_STRIPPER OFF;SPIRV_REFLECT_EXAMPLES OFF;SPIRV_REFLECT_BUILD_TESTS OFF;SPIRV_REFLECT_STATIC_LIB ON")
set(SpirvReflect_FOUND TRUE)