set(srcs wlmon.cpp WLmon_Flash.cpp)
set(include_dirs "include")
set(priv_include_dirs "private_include")
set(reqs esp_partition)
set(priv_reqs wear_levelling)
#set(priv_reqs spi_flash wear_levelling)

idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    list(APPEND srcs "wlmon_linux.cpp")

    #idf_component_get_property(hal_dir hal COMPONENT_DIR)
    #idf_component_get_property(bootloader_support_dir bootloader_support COMPONENT_DIR)
    #list(APPEND include_dirs include ${hal_dir}/include ${bootloader_support_dir}/include)
    #list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/mocks/freertos/")

    list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/mocks/spi_flash/")
else()
    list(APPEND srcs "wlmon_target.cpp")
    list(APPEND priv_reqs "spi_flash")
endif()

idf_component_register(SRCS "${srcs}"
    INCLUDE_DIRS ${include_dirs}
    PRIV_INCLUDE_DIRS ${priv_include_dirs}
    REQUIRES ${reqs}
    PRIV_REQUIRES ${priv_reqs})

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")