option(LIBEMPP_BUILD_SUBMODEL_LIBGS
	"-- ${PRO_NAME}: Build submodel LibGS." ON
)
set(LIBEMPP_LIBGS_INSTALL_PREFIX "" CACHE PATH
	"Install prefix of an external LibGS package."
)
if (LIBEMPP_BUILD_SUBMODEL_LIBGS)
	if (LIBEMPP_LIBGS_INSTALL_PREFIX)
		message(FATAL_ERROR
			"${PRO_NAME}: LIBEMPP_LIBGS_INSTALL_PREFIX is only valid when "
			"LIBEMPP_BUILD_SUBMODEL_LIBGS=OFF."
		)
	endif ()

	if (DEFINED LIBGS_BUILD_CORO AND NOT LIBGS_BUILD_CORO)
		message(FATAL_ERROR
			"${PRO_NAME}: The bundled LibGS requires LIBGS_BUILD_CORO=ON."
		)
	endif ()

	if (DEFINED LIBGS_BUILD_UTILITIES AND NOT LIBGS_BUILD_UTILITIES)
		message(FATAL_ERROR
			"${PRO_NAME}: The bundled LibGS requires LIBGS_BUILD_UTILITIES=ON."
		)
	endif ()

	set(LIBGS_BUILD_CORO ON)
	set(LIBGS_BUILD_UTILITIES ON)

	add_subdirectory(libgs)
	message(STATUS "${PRO_NAME}: Use submodel libgs.")

else ()
	if (LIBEMPP_LIBGS_INSTALL_PREFIX)
		if (NOT IS_ABSOLUTE "${LIBEMPP_LIBGS_INSTALL_PREFIX}")
			message(FATAL_ERROR
				"${PRO_NAME}: LIBEMPP_LIBGS_INSTALL_PREFIX must be an absolute path."
			)
		endif ()

		if (NOT IS_DIRECTORY "${LIBEMPP_LIBGS_INSTALL_PREFIX}")
			message(FATAL_ERROR
				"${PRO_NAME}: LIBEMPP_LIBGS_INSTALL_PREFIX does not exist: "
				"${LIBEMPP_LIBGS_INSTALL_PREFIX}"
			)
		endif ()

		find_package(LibGS CONFIG REQUIRED COMPONENTS coro utils
			PATHS "${LIBEMPP_LIBGS_INSTALL_PREFIX}"
			NO_DEFAULT_PATH
		)
	else ()
		find_package(LibGS CONFIG REQUIRED COMPONENTS coro utils)
	endif ()

	foreach(libempp_libgs_target
		LibGS::core LibGS::coro LibGS::utils
		gs.core gs.coro gs.utils
	)
		if (TARGET ${libempp_libgs_target})
			set_property(TARGET ${libempp_libgs_target} PROPERTY
				IMPORTED_GLOBAL TRUE
			)
		endif ()
	endforeach()

	message(STATUS "${PRO_NAME}: Use external LibGS ${LibGS_VERSION}.")
endif ()
