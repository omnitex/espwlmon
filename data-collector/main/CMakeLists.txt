set(srcs "wlmon.cpp" "WLmon_Flash.cpp" "main.cpp")
set(include_dirs "include")
set(priv_include_dirs "private_include")

idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    list(APPEND srcs "wlmon_linux.cpp")
else()
    list(APPEND srcs "wlmon_target.cpp")
endif()

idf_component_register(SRCS "${srcs}"
    INCLUDE_DIRS ${include_dirs}
    PRIV_INCLUDE_DIRS ${priv_include_dirs})

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")