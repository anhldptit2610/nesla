add_library(neslacore cpu.c
                      mmu.c
                      cart.c
                      ppu.c
                      apu.c
                      mapper.c
                      interrupt.c)

target_include_directories(neslacore PUBLIC ${PROJECT_SOURCE_DIR}/core/)

option(DEBUGGING OFF)
if (DEBUGGING)
    add_definitions(-DCYCLE_DEBUG=1)
endif()