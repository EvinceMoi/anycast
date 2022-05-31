file(GLOB AC_SOURCES
	CONFIGURE_DEPENDS

	*.hpp
	*.cpp
)


add_executable(anycast ${AC_SOURCES})
target_include_directories(anycast PUBLIC 
	${Boost_INCLUDE_DIRS} 
	src 
	${OPENSSL_INCLUDE_DIR}
)
target_link_libraries(anycast 
	Boost::program_options
	Boost::filesystem
	Boost::context
	# Boost::date_time
	Boost::json
	Boost::coroutine
	# Boost::nowide
	${OPENSSL_LIBRARIES}
	fmt
)
# if(COMPILER_HAS_LTO_SUPPORT)
#   set_target_properties(anycast PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
# endif()
if(MSVC)
	# target_link_libraries(anycast Secur32.lib Winmm.lib Mswsock.lib crypt32.lib ws2_32.lib)
endif()
