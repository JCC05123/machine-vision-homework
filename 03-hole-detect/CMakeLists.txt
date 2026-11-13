cmake_minimum_required(VERSION 3.20)
project(HoleDetect LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenCV 预编译库位置：改成你电脑上 OpenCV 解压后的 build 目录
if(NOT OpenCV_DIR)
  set(OpenCV_DIR "D:/opencv/build")
endif()
find_package(OpenCV REQUIRED)

add_executable(HoleDetect main.cpp)
target_link_libraries(HoleDetect PRIVATE ${OpenCV_LIBS})
target_include_directories(HoleDetect PRIVATE ${OpenCV_INCLUDE_DIRS})

# 源码里有中文注释，MSVC 需要 /utf-8
if(MSVC)
  target_compile_options(HoleDetect PRIVATE /utf-8)
endif()

# 把 OpenCV 的 dll 拷到 exe 旁边，双击 exe 也能跑（不依赖 PATH）
add_custom_command(TARGET HoleDetect POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${OpenCV_DIR}/x64/vc16/bin"
          "$<TARGET_FILE_DIR:HoleDetect>"
  COMMENT "Copying OpenCV DLLs next to the executable")

# VS 里 F5 的工作目录 = 工程根目录，这样 images/ 和 result/ 的相对路径才对
set_target_properties(HoleDetect PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# 打开解决方案时默认启动本项目，而不是 CMake 的 ALL_BUILD（否则 F5 会报"拒绝访问"）
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT HoleDetect)
