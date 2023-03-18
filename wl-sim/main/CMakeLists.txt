idf_component_register(SRCS "main.cpp" "random.cpp" "WL_Flash.cpp"
                        INCLUDE_DIRS "include")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
