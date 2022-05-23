#####################
# ConfigVersion file
##
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/FakerConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
        CMake/FakerConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/FakerConfig.cmake
        INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/FakerConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/FakerConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)