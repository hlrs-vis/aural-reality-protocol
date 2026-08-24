cmake_minimum_required (VERSION 3.20)

project (aural_reality_rpc_generator LANGUAGES CXX)

set (protoc_executable $<TARGET_FILE:protoc>)

set (outputdir ${CMAKE_CURRENT_BINARY_DIR}/rpc_options_generated)
file (MAKE_DIRECTORY ${outputdir})

set (RPC_OPTIONS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../../proto/rpc_options.proto)

add_custom_command (
    OUTPUT ${outputdir}/rpc_options.pb.h ${outputdir}/rpc_options.pb.cc
    COMMAND
        ${protoc_executable} ARGS -I${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protobuf/src --cpp_out
        "${outputdir}" -I "${CMAKE_CURRENT_SOURCE_DIR}/../../proto" "${RPC_OPTIONS_FILE}"
    DEPENDS "${RPC_OPTIONS_FILE}"
    COMMENT "Generating sources from ${RPC_OPTIONS_FILE}"
)

add_library (rpc_options_proto STATIC)

target_sources (
    rpc_options_proto PRIVATE ${outputdir}/rpc_options.pb.cc ${outputdir}/rpc_options.pb.h
)

target_link_libraries (rpc_options_proto PUBLIC protobuf::libprotobuf-lite)

target_include_directories (
    rpc_options_proto
    PUBLIC ${outputdir} ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/protobuf/third_party/utf8_range
)

add_executable (protoc-gen-arrpc generator.cpp)

target_link_libraries (
    protoc-gen-arrpc PRIVATE rpc_options_proto protobuf::libprotoc protobuf::libprotobuf
)

target_compile_features (protoc-gen-arrpc PRIVATE cxx_std_20)
