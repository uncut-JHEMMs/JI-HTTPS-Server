project(datagen-tests LANGUAGES CXX)

find_package(Catch2 REQUIRED)

add_executable(datagen-tests src/main.cpp)
target_link_libraries(datagen-tests PRIVATE datagen Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(datagen-tests)
