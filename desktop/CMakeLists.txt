# make sure SDL2 is in the system
find_package(SDL2 REQUIRED)
if (NOT ${SDL2_FOUND})
    message(FATAL_ERROR "SDL2 needs to be installed first")
endif()

add_executable(nesla main.cpp)

target_link_libraries(nesla PRIVATE neslacore
                                    imgui
                                    SDL2)