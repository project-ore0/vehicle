

# 1) Discover all .proto files under your component
discover_proto_files(
    PROTO_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/proto"
    GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

# 2) Register the component, including C++ sources (.cc) and headers
idf_component_register(
    SRCS "${PROTO_SRCS}"          # generated pb.cc files
         "src/libprotobuf-lite.a" # or prebuilt library
    INCLUDE_DIRS "include"
    PRIV_REQUIRES
        # list any IDF or other components
)

# 3) Add host-side protoc generation target
add_proto_generation()
