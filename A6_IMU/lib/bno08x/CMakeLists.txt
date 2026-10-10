# bno08x: polled I2C driver for the BNO080/BNO085/BNO086 IMU (see README.md).
# In your project's CMakeLists.txt:
#   add_subdirectory(lib/bno08x)
#   target_link_libraries(<your project> ... hardware_i2c bno08x)
add_library(bno08x INTERFACE)

target_sources(bno08x INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/bno08x.c)

target_include_directories(bno08x INTERFACE
        ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(bno08x INTERFACE
        pico_stdlib hardware_i2c)
