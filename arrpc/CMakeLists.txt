cmake_minimum_required (VERSION 3.22)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

project (arrpc LANGUAGES CXX)

option (ARRPC_DISABLE_ALL_WARNINGS "Disable all warnings when building arrpc" OFF)

if (ARRPC_DISABLE_ALL_WARNINGS)
    if (MSVC)
        add_compile_options (/W0)
    else ()
        add_compile_options (-w)
    endif ()
endif ()

# Configure abseil
set (ABSL_MSVC_STATIC_RUNTIME ON)
add_subdirectory (
    ${CMAKE_CURRENT_SOURCE_DIR}/../modules/abseil-cpp ${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp
    EXCLUDE_FROM_ALL
)

# Configure protobuf
set (protobuf_INSTALL OFF)
set (protobuf_BUILD_SHARED_LIBS OFF)
add_subdirectory (
    ${CMAKE_CURRENT_SOURCE_DIR}/../modules/protobuf ${CMAKE_CURRENT_BINARY_DIR}/protobuf
    EXCLUDE_FROM_ALL
)

# Add protoc-gen-arrpc plugin
add_subdirectory (
    ${CMAKE_CURRENT_SOURCE_DIR}/protoc-gen-arrpc ${CMAKE_CURRENT_BINARY_DIR}/protoc-gen-arrpc
    EXCLUDE_FROM_ALL
)

add_library (arrpc STATIC)

# Collect all .proto files
set (PROTO_INPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/../proto)
file (GLOB_RECURSE PROTO_FILES "${PROTO_INPUT_DIR}/*.proto")
message (STATUS "Found proto files: ${PROTO_FILES}")

set (outputdir ${CMAKE_CURRENT_BINARY_DIR}/generated/arrpc)
file (MAKE_DIRECTORY ${outputdir})

set (protoc_executable $<TARGET_FILE:protoc>)
set (arrpc_generator $<TARGET_FILE:protoc-gen-arrpc>)

set (SERVICE_FILES ${PROTO_INPUT_DIR}/tmt_service.proto ${PROTO_INPUT_DIR}/dat_service.proto)
foreach (proto IN LISTS PROTO_FILES)
    cmake_path (REMOVE_EXTENSION proto LAST_ONLY OUTPUT_VARIABLE stem)

    # get relative path to the proto file from the protodir and store in REL_PATH
    file (RELATIVE_PATH rel_path ${PROTO_INPUT_DIR} ${stem})

    set (stem ${outputdir}/${rel_path})

    set (generated_files ${stem}.pb.h ${stem}.pb.cc)
    if (proto IN_LIST SERVICE_FILES)
        list (APPEND generated_files ${stem}.rpc.h ${stem}.rpc.cc)
    endif ()

    add_custom_command (
        OUTPUT ${generated_files}
        COMMAND
            ${protoc_executable} ARGS -I${CMAKE_CURRENT_LIST_DIR}/../modules/protobuf/src --cpp_out
            "${outputdir}" -I "${PROTO_INPUT_DIR}" --plugin="${arrpc_generator}"
            --arrpc_out="${outputdir}" "${proto}"
        DEPENDS "${proto}"
        COMMENT "Generating sources from ${proto}"
    )

    target_sources (arrpc PRIVATE ${generated_files})
endforeach ()

# add custom target to clean generated files
get_target_property (PROTO_SOURCES arrpc SOURCES)
add_custom_target (
    CleanAuralRealityProtocol COMMAND ${CMAKE_COMMAND} -E rm -f ${PROTO_SOURCES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries (arrpc PUBLIC libprotobuf-lite utf8_validity)
target_include_directories (arrpc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${outputdir}/..)
