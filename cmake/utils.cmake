# enable_auto_test_command
function (enable_auto_test_command target_name tests_regex)
    if (psqlxx_WANT_AUTO_TESTS)
        add_custom_command(
            TARGET ${target_name}
            POST_BUILD
            COMMAND ctest --output-on-failure -R ${tests_regex}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Testing '${target_name}'"
            VERBATIM)
    endif ()
endfunction ()

# add_gtest_for
function (add_gtest_for source_name)
    if (psqlxx_WANT_TESTS)
        set(test_source_name "${source_name}.test")
        set(target_name "psqlxx.${test_source_name}")
        add_executable(${target_name} ${test_source_name}.cpp)
        target_link_libraries(${target_name} PRIVATE gtest_main ${ARGN})
        target_compile_options(${target_name} PRIVATE ${COMPILER_WARNING_OPTIONS})

        add_test(NAME ${target_name} COMMAND ${target_name})

        enable_auto_test_command(${target_name} ^${target_name}$)
    endif ()
endfunction ()

# discover_gtest_for
function (discover_gtest_for source_name)
    if (psqlxx_WANT_TESTS)
        set(test_source_name "${source_name}.test")
        set(target_prefix "psqlxx.${source_name}.")
        set(target_name "${target_prefix}test")
        add_executable(${target_name} ${test_source_name}.cpp)
        target_link_libraries(${target_name} PRIVATE gtest_main ${ARGN})
        target_compile_options(${target_name} PRIVATE ${COMPILER_WARNING_OPTIONS})

        gtest_discover_tests(
            ${target_name}
            TEST_PREFIX ${target_prefix}
            PROPERTIES LABELS ${PROJECT_NAME})

        enable_auto_test_command(${target_name} ^${target_prefix})
    endif ()
endfunction ()

# config_compiler_and_linker
macro (CONFIG_CXX_COMPILER_AND_LINKER std)
    set(CMAKE_CXX_STANDARD ${std})
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    include(CheckIPOSupported)
    check_ipo_supported(RESULT result)
    if (result)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    endif ()
endmacro ()
