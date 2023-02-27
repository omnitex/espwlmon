set(srcs "wlmon.cpp" "WLmon_Flash.cpp" "main.cpp")
set(include_dirs "include")
set(reqs esp_partition)
set(priv_reqs wear_levelling spi_flash)

idf_build_get_property(target IDF_TARGET)
idf_component_get_property(wl_dir wear_levelling COMPONENT_DIR)
idf_component_get_property(hal_dir hal COMPONENT_DIR)

list(APPEND priv_include_dirs ${wl_dir}/private_include ${hal_dir}/include/hal/)

if(${target} STREQUAL "linux")
    list(APPEND srcs "wlmon_linux.cpp")
else()
    list(APPEND srcs "wlmon_target.cpp")
endif()

idf_component_register(SRCS "${srcs}"
    INCLUDE_DIRS ${include_dirs}
    PRIV_INCLUDE_DIRS ${priv_include_dirs}
    REQUIRES ${reqs}
    PRIV_REQUIRES ${priv_reqs})

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")