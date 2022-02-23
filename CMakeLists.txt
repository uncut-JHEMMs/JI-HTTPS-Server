cmake_minimum_required(VERSION 3.15)

project(HTTPS-Server VERSION 0.1.0 LANGUAGES CXX)
message(STATUS "Started CMake for ${PROJECT_NAME} v${PROJECT_VERSION}...\n")

# Use ccache to speed up rebuilds if it's available.
find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

# Prevent builds in the source directory
if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there.\n")
endif()

set(PROJECT_SOURCES
    src/main.cpp
    src/env.cpp
    src/server_opts.cpp
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

set_target_properties(
    ${PROJECT_NAME}
      PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}"
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

if (MSVC)
    set(PROJECT_WARNINGS
        /W4          # Baseline reasonable warnings
        /w14242      # 'identifier': conversion from 'type1' to 'type1', possible loss
                     # of data
        /w14254      # 'operator': conversion from 'type1:field_bits' to
                     # 'type2:field_bits', possible loss of data
        /w14263      # 'function': member function does not override any base class
                     # virtual member function
        /w14265      # 'classname': class has virtual functions, but destructor is not
                     # virtual instances of this class may not be destructed correctly
        /w14287      # 'operator': unsigned/negative constant mismatch
        /we4289      # nonstandard extension used: 'variable': loop control variable
                     # declared in the for-loop is used outside the for-loop scope
        /w14296      # 'operator': expression is always 'boolean_value'
        /w14311      # 'variable': pointer truncation from 'type1' to 'type2'
        /w14545      # expression before comma evaluates to a function which is missing
                     # an argument list
        /w14546      # function call before comma missing argument list
        /w14547      # 'operator': operator before comma has no effect; expected
                     # operator with side-effect
        /w14549      # 'operator': operator before comma has no effect; did you intend
                     # 'operator'?
        /w14555      # expression has no effect; expected expression with side- effect
        /w14619      # pragma warning: there is no warning number 'number'
        /w14640      # Enable warning on thread un-safe static member initialization
        /w14826      # Conversion from 'type1' to 'type_2' is sign-extended. This may
                     # cause unexpected runtime behavior.
        /w14905      # wide string literal cast to 'LPSTR'
        /w14906      # string literal cast to 'LPWSTR'
        /w14928      # illegal copy-initialization; more than one user-defined
                     # conversion has been implicitly applied
        /permissive- # standards conformance mode for MSVC compiler.
    )
elseif (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(PROJECT_WARNINGS
        -Wall
        -Wextra              # Reasonable and standard
        -Wshadow             # Warn the user if a variable declaration shadows one from a
                             # parent context
        -Wnon-virtual-dtor   # Warn the user if a class with virtual functions has a
                             # non-virtual destructor. This helps catch hard to
                             # track down memory errors
        -Wcast-align         # Warn for potential performance problem casts
        -Wunused             # Warn on anything being unused
        -Woverloaded-virtual # Warn if you overload (not override) a virtual function
        -Wpedantic           # Warn if non-standard C++ is used
        -Wconversion         # Warn on type conversions that may lose data
        -Wsign-conversion    # Warn on sign conversions
        -Wnull-dereference   # Warn if a null dereference is detected
        -Wformat=2
    )

    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(PROJECT_WARNINGS
            ${PROJECT_WARNINGS}
            -Wduplicated-cond     # Warn if if / else chain has duplicated conditions
            -Wduplicated-branches # Warn if if / else chain has duplicated code
            -Wuseless-cast        # Warn if you perform a cast to the same type
        )
    endif()
endif()
target_compile_options(${PROJECT_NAME} PUBLIC ${PROJECT_WARNINGS})

find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBHTTPSERVER REQUIRED libhttpserver)

target_link_libraries(${PROJECT_NAME} ${LIBHTTPSERVER_LIBRARIES} spdlog::spdlog nlohmann_json::nlohmann_json Threads::Threads)
target_include_directories(${PROJECT_NAME} PUBLIC ${LIBHTTPSERVER_INCLUDE_DIRS} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(${PROJECT_NAME} PUBLIC ${LIBHTTPSERVER_CFLAGS_OTHER})
