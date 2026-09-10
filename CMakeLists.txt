cmake_minimum_required(VERSION 3.16)
project(GenLang VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(GENLANG_BUILD_CLI "Build the genlang CLI" ON)
option(GENLANG_BUILD_TESTS "Build the test suite" ON)
option(GENLANG_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

include(GNUInstallDirs)
include(CTest)

set(GENLANG_SOURCES
    src/memory/allocator.c
    src/error/error.c
    src/internal/string.c
    src/internal/vector.c
    src/io/file.c
    src/io/path.c
    src/lexer/lexer.c
    src/ast/ast.c
    src/parser/parser.c
    src/runtime/value.c
    src/runtime/type.c
    src/runtime/set.c
    src/runtime/relation.c
    src/runtime/document.c
    src/semantic/semantic.c
    src/semantic/import.c
    src/query/query.c
    src/serializer/serializer.c
    src/serializer/json.c
    src/serializer/json_import.c
    src/serializer/yaml.c
    src/serializer/yaml_import.c
    src/serializer/binary.c
)

if(MSVC)
    set(GENLANG_WARNINGS /W4)
else()
    set(GENLANG_WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wstrict-prototypes
        -Wmissing-prototypes
    )
endif()

add_library(genlang_objects OBJECT ${GENLANG_SOURCES})
set_target_properties(genlang_objects PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
target_compile_definitions(genlang_objects PRIVATE GENLANG_BUILD_SHARED)
target_compile_options(genlang_objects PRIVATE ${GENLANG_WARNINGS})
target_include_directories(genlang_objects
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/uthash
)

add_library(genlang_static STATIC $<TARGET_OBJECTS:genlang_objects>)
add_library(genlang_shared SHARED $<TARGET_OBJECTS:genlang_objects>)

foreach(lib genlang_static genlang_shared)
    target_include_directories(${lib}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
endforeach()

set_target_properties(genlang_static PROPERTIES
    OUTPUT_NAME genlang
    EXPORT_NAME genlang
)
set_target_properties(genlang_shared PROPERTIES
    OUTPUT_NAME genlang
    EXPORT_NAME genlang_shared
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
target_compile_definitions(genlang_shared INTERFACE GENLANG_SHARED)

add_library(GenLang::genlang ALIAS genlang_static)
add_library(GenLang::genlang_shared ALIAS genlang_shared)

if(GENLANG_ENABLE_SANITIZERS AND NOT MSVC)
    foreach(tgt genlang_objects genlang_static genlang_shared)
        target_compile_options(${tgt} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${tgt} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()

if(GENLANG_BUILD_CLI)
    add_executable(genlang
        cli/main.c
        cli/commands.c
        cli/repl.c
    )
    target_include_directories(genlang PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli)
    target_link_libraries(genlang PRIVATE genlang_static)
    target_compile_options(genlang PRIVATE ${GENLANG_WARNINGS})
    if(GENLANG_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(genlang PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(genlang PRIVATE -fsanitize=address,undefined)
    endif()
    set_target_properties(genlang PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

find_path(GENLANG_NODE_INCLUDE node_api.h
    PATHS /usr/include/node /usr/local/include/node
)
if(GENLANG_NODE_INCLUDE)
    add_library(genlang_node MODULE bindings/node/src/binding.c)
    target_include_directories(genlang_node PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${GENLANG_NODE_INCLUDE}
    )
    target_link_libraries(genlang_node PRIVATE genlang_shared)
    target_compile_definitions(genlang_node PRIVATE NAPI_VERSION=8 BUILDING_NODE_EXTENSION)
    set_target_properties(genlang_node PROPERTIES
        PREFIX ""
        SUFFIX ".node"
        OUTPUT_NAME genlang
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        BUILD_RPATH ${CMAKE_BINARY_DIR}
    )
endif()

if(GENLANG_BUILD_TESTS AND BUILD_TESTING)
    add_executable(genlang_tests
        tests/test_main.c
        tests/test_lexer.c
        tests/test_parser.c
        tests/test_semantic.c
        tests/test_runtime.c
        tests/test_query.c
        tests/test_serializer.c
        tests/test_api.c
        tests/test_integration.c
    )
    target_include_directories(genlang_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/uthash
    )
    target_link_libraries(genlang_tests PRIVATE genlang_static)
    target_compile_options(genlang_tests PRIVATE ${GENLANG_WARNINGS})
    if(GENLANG_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(genlang_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(genlang_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME genlang_tests COMMAND genlang_tests)
    set_tests_properties(genlang_tests PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    find_program(GENLANG_PYTHON3 NAMES python3 python)
    if(GENLANG_PYTHON3)
        add_test(NAME genlang_python_bindings
            COMMAND ${GENLANG_PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/bindings/python/tests/test_genlang.py
        )
        set_tests_properties(genlang_python_bindings PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/bindings/python/src;GENLANG_LIB_DIR=${CMAKE_BINARY_DIR};LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}"
        )
        add_test(NAME genlang_lsp
            COMMAND ${GENLANG_PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/lsp/test_lsp.py
        )
        set_tests_properties(genlang_lsp PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/bindings/python/src;GENLANG_LIB_DIR=${CMAKE_BINARY_DIR};LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}"
        )
    endif()

    find_program(GENLANG_CARGO cargo)
    if(GENLANG_CARGO)
        add_test(NAME genlang_rust_bindings
            COMMAND ${GENLANG_CARGO} test --manifest-path ${CMAKE_CURRENT_SOURCE_DIR}/bindings/rust/Cargo.toml
        )
        set_tests_properties(genlang_rust_bindings PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            ENVIRONMENT "GENLANG_LIB_DIR=${CMAKE_BINARY_DIR};CARGO_TARGET_DIR=${CMAKE_BINARY_DIR}/rust-target"
        )
    endif()

    find_program(GENLANG_GO go)
    if(GENLANG_GO)
        add_test(NAME genlang_go_bindings
            COMMAND ${GENLANG_GO} test
        )
        set_tests_properties(genlang_go_bindings PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/go
            ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR};CGO_LDFLAGS=-L${CMAKE_BINARY_DIR} -lgenlang"
        )
    endif()

    find_program(GENLANG_NODE node)
    if(GENLANG_NODE AND TARGET genlang_node)
        add_test(NAME genlang_node_bindings
            COMMAND ${GENLANG_NODE} ${CMAKE_CURRENT_SOURCE_DIR}/bindings/node/test.js
        )
        set_tests_properties(genlang_node_bindings PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            ENVIRONMENT "GENLANG_NODE_MODULE=${CMAKE_BINARY_DIR}/genlang.node;LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}"
        )
    endif()

        find_program(GENLANG_DOTNET dotnet HINTS ${CMAKE_BINARY_DIR}/dotnet-sdk)
    if(GENLANG_DOTNET)
        get_filename_component(GENLANG_DOTNET_ROOT ${GENLANG_DOTNET} DIRECTORY)
        add_test(NAME genlang_csharp_bindings
            COMMAND ${GENLANG_DOTNET} run --project ${CMAKE_CURRENT_SOURCE_DIR}/bindings/csharp/GenLang.Tests/GenLang.Tests.csproj
        )
        set_tests_properties(genlang_csharp_bindings PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR};DOTNET_ROOT=${GENLANG_DOTNET_ROOT};DOTNET_CLI_HOME=${CMAKE_BINARY_DIR}/dotnet-home;DOTNET_CLI_TELEMETRY_OPTOUT=1"
        )
    endif()

    find_program(GENLANG_JAVAC javac HINTS ${CMAKE_BINARY_DIR}/jdk/bin $ENV{JAVA_HOME}/bin)
    find_program(GENLANG_JAVA java HINTS ${CMAKE_BINARY_DIR}/jdk/bin $ENV{JAVA_HOME}/bin)
    find_file(GENLANG_JNA_JAR jna.jar
        PATHS ${CMAKE_BINARY_DIR}/jna ${CMAKE_SOURCE_DIR}/third_party/jna
    )
    if(GENLANG_JAVA AND GENLANG_JAVAC AND GENLANG_JNA_JAR)
        add_test(NAME genlang_java_bindings
            COMMAND ${CMAKE_COMMAND} -E env
                "GENLANG_JAVAC=${GENLANG_JAVAC}"
                "GENLANG_JAVA=${GENLANG_JAVA}"
                "GENLANG_JNA_JAR=${GENLANG_JNA_JAR}"
                "GENLANG_JAVA_CLASSES=${CMAKE_BINARY_DIR}/java-classes"
                "GENLANG_LIB_DIR=${CMAKE_BINARY_DIR}"
                "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}"
                bash ${CMAKE_CURRENT_SOURCE_DIR}/bindings/java/run-tests.sh
        )
        set_tests_properties(genlang_java_bindings PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
    endif()
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/genlang.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/genlang.pc
    @ONLY
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/genlangConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/GenLangConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GenLang
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/GenLangConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(TARGETS genlang_static genlang_shared
    EXPORT GenLangTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(GENLANG_BUILD_CLI)
    install(TARGETS genlang RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(FILES include/genlang.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT GenLangTargets
    FILE GenLangTargets.cmake
    NAMESPACE GenLang::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GenLang
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/GenLangConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/GenLangConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GenLang
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/genlang.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
