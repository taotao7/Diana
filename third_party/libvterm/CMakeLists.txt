cmake_minimum_required(VERSION 3.20)

add_library(vterm STATIC
    src/encoding.c
    src/keyboard.c
    src/mouse.c
    src/parser.c
    src/pen.c
    src/screen.c
    src/state.c
    src/unicode.c
    src/vterm.c
)

target_include_directories(vterm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(vterm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(vterm PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

if(NOT MSVC)
    target_compile_options(vterm PRIVATE -Wno-unused-parameter)
endif()
