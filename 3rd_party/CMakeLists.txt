option(LIBEMPP_USE_EMBEDDED_RIWO
	"-- ${PRO_NAME}: Use the embedded Riwo dependency." ON
)
set(LIBEMPP_RIWO_INSTALL_PREFIX "" CACHE PATH
	"Install prefix of an external Riwo package."
)
if (LIBEMPP_USE_EMBEDDED_RIWO)
	if (LIBEMPP_RIWO_INSTALL_PREFIX)
		message(FATAL_ERROR
			"${PRO_NAME}: LIBEMPP_RIWO_INSTALL_PREFIX is only valid when "
			"LIBEMPP_USE_EMBEDDED_RIWO=OFF."
		)
	endif ()

	if (DEFINED RIWO_BUILD_CORO AND NOT RIWO_BUILD_CORO)
		message(FATAL_ERROR
			"${PRO_NAME}: The bundled Riwo requires RIWO_BUILD_CORO=ON."
		)
	endif ()

	if (DEFINED RIWO_BUILD_UTILITIES AND NOT RIWO_BUILD_UTILITIES)
		message(FATAL_ERROR
			"${PRO_NAME}: The bundled Riwo requires RIWO_BUILD_UTILITIES=ON."
		)
	endif ()

	set(RIWO_BUILD_CORO ON)
	set(RIWO_BUILD_UTILITIES ON)

	add_subdirectory(riwo)
	message(STATUS "${PRO_NAME}: Use embedded Riwo.")

else ()
	if (LIBEMPP_RIWO_INSTALL_PREFIX)
		if (NOT IS_ABSOLUTE "${LIBEMPP_RIWO_INSTALL_PREFIX}")
			message(FATAL_ERROR
				"${PRO_NAME}: LIBEMPP_RIWO_INSTALL_PREFIX must be an absolute path."
			)
		endif ()

		if (NOT IS_DIRECTORY "${LIBEMPP_RIWO_INSTALL_PREFIX}")
			message(FATAL_ERROR
				"${PRO_NAME}: LIBEMPP_RIWO_INSTALL_PREFIX does not exist: "
				"${LIBEMPP_RIWO_INSTALL_PREFIX}"
			)
		endif ()

		find_package(Riwo CONFIG REQUIRED COMPONENTS coro utils
			PATHS "${LIBEMPP_RIWO_INSTALL_PREFIX}"
			NO_DEFAULT_PATH
		)
	else ()
		find_package(Riwo CONFIG REQUIRED COMPONENTS coro utils)
	endif ()

	foreach(libempp_riwo_target
		Riwo::core Riwo::coro Riwo::utils
		riwo.core riwo.coro riwo.utils
	)
		if (TARGET ${libempp_riwo_target})
			set_property(TARGET ${libempp_riwo_target} PROPERTY
				IMPORTED_GLOBAL TRUE
			)
		endif ()
	endforeach()

	message(STATUS "${PRO_NAME}: Use external Riwo ${Riwo_VERSION}.")
endif ()

option(LIBEMPP_USE_EMBEDDED_NLOHMANN
	"-- ${PRO_NAME}: Use the embedded nlohmann/json dependency." ON
)
set(LIBEMPP_NLOHMANN_INSTALL_PREFIX "" CACHE PATH
	"Install prefix of an external nlohmann/json package."
)
if (LIBEMPP_USE_EMBEDDED_NLOHMANN)
	if (LIBEMPP_NLOHMANN_INSTALL_PREFIX)
		message(FATAL_ERROR
			"${PRO_NAME}: LIBEMPP_NLOHMANN_INSTALL_PREFIX is only valid when "
			"LIBEMPP_USE_EMBEDDED_NLOHMANN=OFF."
		)
	endif ()

	add_subdirectory(nlohmann.json)
	message(STATUS "${PRO_NAME}: Use embedded nlohmann/json.")

else ()
	if (LIBEMPP_NLOHMANN_INSTALL_PREFIX)
		if (NOT IS_ABSOLUTE "${LIBEMPP_NLOHMANN_INSTALL_PREFIX}")
			message(FATAL_ERROR
				"${PRO_NAME}: LIBEMPP_NLOHMANN_INSTALL_PREFIX must be an absolute path."
			)
		endif ()

		if (NOT IS_DIRECTORY "${LIBEMPP_NLOHMANN_INSTALL_PREFIX}")
			message(FATAL_ERROR
				"${PRO_NAME}: LIBEMPP_NLOHMANN_INSTALL_PREFIX does not exist: "
				"${LIBEMPP_NLOHMANN_INSTALL_PREFIX}"
			)
		endif ()

		find_package(nlohmann_json CONFIG REQUIRED
			PATHS "${LIBEMPP_NLOHMANN_INSTALL_PREFIX}"
			NO_DEFAULT_PATH
		)
	else ()
		find_package(nlohmann_json CONFIG REQUIRED)
	endif ()

	set_property(TARGET nlohmann_json::nlohmann_json PROPERTY
		IMPORTED_GLOBAL TRUE
	)
	message(STATUS
		"${PRO_NAME}: Use external nlohmann/json ${nlohmann_json_VERSION}."
	)
endif ()
