cmake_minimum_required(VERSION 3.10)
project(modem_5g LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(MODEM_5G_SOURCES
    src/modem_5g_core.c
    src/drivers/drv_uart_mr880a.c
)

# find_program(UDHCPC_EXECUTABLE udhcpc)
# if(NOT UDHCPC_EXECUTABLE)
#     message(FATAL_ERROR "udhcpc not found. Install with: apt install -y udhcpc")
# endif()

add_library(modem_5g SHARED ${MODEM_5G_SOURCES})

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(modem_5g PRIVATE
        -Wall
        -Wextra
    )
endif()

install(TARGETS modem_5g
    EXPORT ${SROBOTIS_EXPORT_SET}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES include/5g.h
    DESTINATION include
)

option(BUILD_TESTS "Build test programs" OFF)

if(BUILD_TESTS)
    add_executable(test_5g_mr880a test/test_5g_mr880a.c)
    target_link_libraries(test_5g_mr880a modem_5g)
    set_target_properties(test_5g_mr880a PROPERTIES BUILD_RPATH "$ORIGIN/../lib")
    install(TARGETS test_5g_mr880a RUNTIME DESTINATION bin)
endif()
