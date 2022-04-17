project(datagen-tests LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v2.13.9
)
set(CATCH_BUILD_STATIC_LIBRARY ON)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/contrib")

add_executable(datagen-tests src/main.cpp)
target_link_libraries(datagen-tests PUBLIC Catch2::Catch2 PRIVATE datagen)

include(CTest)
include(Catch)
catch_discover_tests(datagen-tests)
