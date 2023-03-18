set(srcs "main.cpp" "random.cpp" "WL_Flash.cpp")

idf_component_register(SRCS ${srcs}
                        INCLUDE_DIRS "include")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
