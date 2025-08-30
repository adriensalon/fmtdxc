cmake_minimum_required(VERSION 3.12)
project(fmtdxc)

option(FMTDXC_BUILD_TOOL "Build tool executables" ON)

if(!CEREAL_INCLUDE_DIR)
    message(FATAL_ERROR "Please provide the directory to cereal include dir by setting CEREAL_INCLUDE_DIR")
endif()

# fmtdxc library
file(GLOB_RECURSE fmtals_source "source/*.cpp")
add_library(fmtdxc STATIC ${fmtals_source})
set_target_properties(fmtdxc PROPERTIES CXX_STANDARD 17)
target_include_directories(fmtdxc PUBLIC include)
target_include_directories(fmtdxc PRIVATE ${CEREAL_INCLUDE_DIR})

# tools
if(FMTDXC_BUILD_TOOL)
    add_executable(dxcc2json "tool/dxcc2json.cpp")
    set_target_properties(dxcc2json PROPERTIES CXX_STANDARD 17)
    target_link_libraries(dxcc2json PRIVATE fmtdxc)
    add_executable(json2dxcc "tool/json2dxcc.cpp")
    set_target_properties(json2dxcc PROPERTIES CXX_STANDARD 17)
    target_link_libraries(json2dxcc PRIVATE fmtdxc)
endif()
