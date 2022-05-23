cmake_minimum_required(VERSION 3.21)
project(faker_cpp)

set(CMAKE_CXX_STANDARD 17)

add_library(faker_cpp INTERFACE)
target_compile_features(faker_cpp INTERFACE cxx_std_17)
target_include_directories(faker_cpp
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

include(GNUInstallDirs)

set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/Faker)

install(
    TARGETS
        faker_cpp
    EXPORT
        FakerTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(EXPORT FakerTargets
    FILE FakerTargets.cmake
    NAMESPACE Faker::
    DESTINATION ${INSTALL_CONFIGDIR})

install(DIRECTORY faker/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/faker
    FILES_MATCHING PATTERN "include/*.hpp")

add_library(Faker::Faker ALIAS faker_cpp)