cmake_minimum_required(VERSION 3.12)
project(fmtdxc)

# fmtdxc library
file(GLOB_RECURSE fmtals_source "source/*.cpp")
add_library(fmtdxc STATIC ${fmtals_source})
set_target_properties(fmtdxc PROPERTIES CXX_STANDARD 17)
target_include_directories(fmtdxc PRIVATE external/cereal)
target_include_directories(fmtdxc PUBLIC include)
target_link_libraries(fmtdxc PRIVATE cereal)
