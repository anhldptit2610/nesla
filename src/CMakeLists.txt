add_library(neslacore cpu.c
                      mmu.c)

target_include_directories(neslacore PUBLIC ${PROJECT_SOURCE_DIR}/include)