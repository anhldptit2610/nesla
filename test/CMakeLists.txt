add_executable(cpu_test cpu_test.c)

target_link_libraries(cpu_test PRIVATE neslacore
                                        cjson)
                                    
option(DEBUGGING OFF)
if (DEBUGGING)
    add_definitions(-DCYCLE_DEBUG=1)
endif()