# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/tracy-src")
  file(MAKE_DIRECTORY "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/tracy-src")
endif()
file(MAKE_DIRECTORY
  "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/tracy-build"
  "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/tracy-subbuild/tracy-populate-prefix"
  "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/tracy-subbuild/tracy-populate-prefix/tmp"
  "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/tracy-subbuild/tracy-populate-prefix/src/tracy-populate-stamp"
  "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/tracy-subbuild/tracy-populate-prefix/src"
  "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/tracy-subbuild/tracy-populate-prefix/src/tracy-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/tracy-subbuild/tracy-populate-prefix/src/tracy-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/tracy-subbuild/tracy-populate-prefix/src/tracy-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
