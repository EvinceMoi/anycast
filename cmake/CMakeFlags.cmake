set(CMAKE_CXX_SOURCE_FILE_EXTENSIONS cpp;c++;cc)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
if (WIN32)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/debug)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/release)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
message(STATUS ">> export compile commands: ON")

set(CMAKE_C_STANDARD 11)

set(CMAKE_CXX_EXTENSIONS OFF) # change -std=gnu++20 to -std=c++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS ">> force cxx standard: ${CMAKE_CXX_STANDARD}")
