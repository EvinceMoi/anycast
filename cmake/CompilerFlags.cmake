
include(CheckCXXCompilerFlag)
include(CheckIncludeFileCXX)
include(CheckIPOSupported) # for lto

check_cxx_compiler_flag(-fvisibility-inlines-hidden COMPILER_HAS_VISIBILITY_INLINE_HIDDEN)
check_cxx_compiler_flag(-fvisibility=hidden COMPILER_HAS_VISIBILITY_HIDDEN)
check_cxx_compiler_flag(-fdiagnostics-color=always COMPILER_HAS_COLOR)
check_cxx_compiler_flag(-fcoroutines COMPILER_HAS_FCOROUTINES)
check_cxx_compiler_flag(-fcoroutines-ts COMPILER_HAS_FCOROUTINES_TS)
check_ipo_supported(RESULT COMPILER_HAS_LTO_SUPPORT)

if (COMPILER_HAS_VISIBILITY_INLINE_HIDDEN)
	# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden>)
endif()

if (COMPILER_HAS_VISIBILITY_HIDDEN)
	add_compile_options(-fvisibility=hidden)
endif()

if (COMPILER_HAS_COLOR)
	add_compile_options(-fdiagnostics-color=always)
endif()

check_include_file_cxx("coroutine" CAN_INCLUDE_STD_COROUTINE)
if (COMPILER_HAS_FCOROUTINES AND NOT WIN32)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fcoroutines>)
endif()
if (COMPILER_HAS_FCOROUTINES_TS AND NOT WIN32)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fcoroutines-ts>)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)
    add_link_options(-stdlib=libc++)
    # add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)
    # add_link_options($<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)

    add_link_options(-rtlib=compiler-rt)
endif()

if (COMPILER_HAS_LTO_SUPPORT)
	# set_property(TARGET <target> PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
endif()


# add_definitions(-DBOOST_BEAST_USE_STD_STRING_VIEW)