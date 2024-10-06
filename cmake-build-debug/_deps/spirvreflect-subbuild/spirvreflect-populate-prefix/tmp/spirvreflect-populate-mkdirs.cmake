# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/spirvreflect-src")
  file(MAKE_DIRECTORY "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/spirvreflect-src")
endif()
file(MAKE_DIRECTORY
  "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/spirvreflect-build"
  "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/spirvreflect-subbuild/spirvreflect-populate-prefix"
  "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/spirvreflect-subbuild/spirvreflect-populate-prefix/tmp"
  "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/spirvreflect-subbuild/spirvreflect-populate-prefix/src/spirvreflect-populate-stamp"
  "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/spirvreflect-subbuild/spirvreflect-populate-prefix/src"
  "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/spirvreflect-subbuild/spirvreflect-populate-prefix/src/spirvreflect-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/spirvreflect-subbuild/spirvreflect-populate-prefix/src/spirvreflect-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/79014/CLionProjects/graphics-course/cmake-build-debug/_deps/spirvreflect-subbuild/spirvreflect-populate-prefix/src/spirvreflect-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
