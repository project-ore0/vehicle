# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/workspaces/ore0-mark3/managed_components/albkharisov__esp_google_protobuf/protobuf/cmake")
  file(MAKE_DIRECTORY "/workspaces/ore0-mark3/managed_components/albkharisov__esp_google_protobuf/protobuf/cmake")
endif()
file(MAKE_DIRECTORY
  "/workspaces/ore0-mark3/build_host_protoc"
  "/workspaces/ore0-mark3/build_host_protoc"
  "/workspaces/ore0-mark3/build_host_protoc/tmp"
  "/workspaces/ore0-mark3/build_host_protoc/src/protoc_host-stamp"
  "/workspaces/ore0-mark3/build_host_protoc/src"
  "/workspaces/ore0-mark3/build_host_protoc/src/protoc_host-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/workspaces/ore0-mark3/build_host_protoc/src/protoc_host-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/workspaces/ore0-mark3/build_host_protoc/src/protoc_host-stamp${cfgdir}") # cfgdir has leading slash
endif()
