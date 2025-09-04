cmake_minimum_required(VERSION 4.0)
project(AsciiPlayerReborn)

include(FetchContent)
FetchContent_Declare(miniaudio
        GIT_REPOSITORY https://github.com/mackron/miniaudio
        GIT_TAG 0.11.22)
FetchContent_MakeAvailable(miniaudio)

set(CMAKE_CXX_STANDARD 20)

find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
        libavdevice
        libavfilter
        libavformat
        libavcodec
        libswresample
        libswscale
        libavutil
)

include_directories(${OpenCV_INCLUDE_DIRS})

add_compile_options(-Wall -Wextra -Wconversion -Wdouble-promotion -Wno-sign-conversion -Wno-unknown-pragmas -Wuninitialized)
add_link_options(-Wall -Wextra -Wconversion -Wdouble-promotion -Wno-sign-conversion -Wno-unknown-pragmas -Wuninitialized)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Compiling Debug version.")
    add_compile_options(-O0 -g3 -fstandalone-debug -fno-omit-frame-pointer -DDEBUG -fno-inline -fstack-protector-all -fsanitize=address,undefined -fsanitize-trap)
    add_link_options(-O0 -g3 -fstandalone-debug -fno-omit-frame-pointer -DDEBUG -fno-inline -fstack-protector-all -fsanitize=address,undefined -fsanitize-trap)
else ()
    add_compile_options(-O3)
endif ()

add_executable(AsciiPlayerReborn src/main.cpp)

target_link_libraries(AsciiPlayerReborn ${OpenCV_LIBS} miniaudio PkgConfig::LIBAV)