set(srcs "wlmon.cpp" "main.cpp")
set(include_dirs "include")

idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    list(APPEND srcs "wlmon_linux.cpp")
else()
    list(APPEND srcs "wlmon_target.cpp")
endif()

idf_component_register(SRCS "${srcs}"
    INCLUDE_DIRS ${include_dirs})
