project(log_generator LANGUAGES CXX)

set(CXX_EXTENSIONS OFF)

add_executable(log_generator src/main.cpp)
target_compile_features(log_generator PUBLIC cxx_std_17)