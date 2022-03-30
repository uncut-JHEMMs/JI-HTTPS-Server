find_package(Boost REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Development.Embed)

find_library(LMDB_LIB NAMES lmdb REQUIRED)
find_path(LMDB_INCLUDE_DIR NAMES lmdb.h REQUIRED)
find_path(LMDB++_INCLUDE_DIR NAMES lmdb++.h REQUIRED)

message(STATUS "Found lmdb (include: ${LMDB_INCLUDE_DIR}, library: ${LMDB_LIB})")
message(STATUS "Found lmdb++ (include: ${LMDB++_INCLUDE_DIR})")

add_library(datagen src/datagen.cpp src/faker.cpp src/generation.cpp)
target_include_directories(datagen PUBLIC ${Boost_INCLUDE_DIRS} ${LMDB_INCLUDE_DIR} ${LMDB++_INCLUDE_DIR})
target_link_libraries(datagen ${Boost_LIBS} ${LMDB_LIB} Python3::Python)
target_compile_features(datagen PUBLIC cxx_std_17)

add_executable(datagen-cli src/main.cpp)
target_link_libraries(datagen-cli datagen)
target_compile_features(datagen-cli PUBLIC cxx_std_17)
