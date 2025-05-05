cmake_minimum_required(VERSION 3.7...3.19)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(ShaderCompile CXX)

option(SC_BUILD_PS1_X_COMPILER "Allow to compile Pixel Shaders 1.x (by DirectX 9 shader compiler)." ON)
option(RE2_BUILD_TESTING "" OFF)

# Abseil install required by re2.
set(ABSL_ENABLE_INSTALL ON)
# Use static MSVC runtime for Abseil to match others.
set(ABSL_MSVC_STATIC_RUNTIME ON)

add_subdirectory(shared/abseil-cpp)
add_subdirectory(shared/re2)
add_subdirectory(shared/gsl)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# https://github.com/izenecloud/cmake/blob/master/SetCompilerWarningAll.cmake
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Use the highest warning level for Visual Studio.
    set(CMAKE_CXX_WARNING_LEVEL 4)

    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
endif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

set(SRC
    ShaderCompile/cfgprocessor.cpp
    ShaderCompile/d3dxfxc.cpp
    ShaderCompile/ShaderCompile.cpp
    ShaderCompile/shaderparser.cpp
    ShaderCompile/utlbuffer.cpp
    )

add_executable(ShaderCompile ${SRC})

set(INCLUDE_DIRS
    ShaderCompile/include
    shared/re2
    )

if(SC_BUILD_PS1_X_COMPILER)
    target_compile_definitions(ShaderCompile PRIVATE SC_BUILD_PS1_X_COMPILER=1)
    list(APPEND INCLUDE_DIRS shared/dx9sdk/include)
    target_link_directories(ShaderCompile PRIVATE shared/dx9sdk/lib/x64)

    add_custom_command(
        TARGET ShaderCompile POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${CMAKE_SOURCE_DIR}/shared/dx9sdk/bin/x64/D3DCompiler_43.dll $<TARGET_FILE_DIR:ShaderCompile>
        WORKING_DIRECTORY $<TARGET_FILE_DIR:ShaderCompile>
        COMMENT "Copy 'shared/dx9sdk/bin/x64/D3DCompiler_43.dll' to '$<TARGET_FILE_DIR:ShaderCompile>' output directory."
    )

    add_custom_command(
        TARGET ShaderCompile POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${CMAKE_SOURCE_DIR}/shared/dx9sdk/bin/x64/D3DX9_43.dll $<TARGET_FILE_DIR:ShaderCompile>
        WORKING_DIRECTORY $<TARGET_FILE_DIR:ShaderCompile>
        COMMENT "Copy 'shared/dx9sdk/bin/x64/D3DX9_43.dll' to '$<TARGET_FILE_DIR:ShaderCompile>' output directory."
    )
endif(SC_BUILD_PS1_X_COMPILER)

target_include_directories(ShaderCompile PRIVATE ${INCLUDE_DIRS})
target_link_libraries(ShaderCompile PRIVATE re2::re2 Microsoft.GSL::GSL)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
set_property(TARGET ShaderCompile PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET re2 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

