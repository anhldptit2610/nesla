add_library(neslacore cpu.c
                      mmu.c)

target_include_directories(neslacore PUBLIC ${PROJECT_SOURCE_DIR}/include)

option(DEBUGGING OFF)
if (DEBUGGING)
    add_definitions(-DCYCLE_DEBUG=1)
endif()