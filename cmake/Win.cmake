if (WIN32)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

	if (MSVC)
		add_compile_options(/bigobj /MP /Zc:__cplusplus /utf-8)
		add_definitions(
			-D_CRT_SECURE_NO_DEPRECATE
			-D_CRT_SECURE_NO_WARNINGS
			-D_CRT_NONSTDC_NO_DEPRECATE
			-D_CRT_NONSTDC_NO_WARNINGS
			-D_SCL_SECURE_NO_DEPRECATE
			-D_SCL_SECURE_NO_WARNINGS
		)

		set(CMAKE_CXX_STACK_SIZE "100000000")
	endif()

	add_definitions(
		-DWIN32_LEAN_AND_MEAN
		-D_WIN32_WINNT=0x0601
		-DNOMINMAX
		-DUNICODE
		-D_UNICODE
		-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
	)

	add_definitions(
		-DBOOST_ALL_STATIC_LINK
		-DBOOST_THREAD_USE_LIB
		-DBOOST_FILESYSTEM_STATIC_LINK
		-DBOOST_USE_WINAPI_VERSION=0x0601
	)

    link_libraries(
		Secur32.lib
		Bcrypt.lib
		Winmm.lib
		Mswsock.lib
	)

endif()