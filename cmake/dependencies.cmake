# SPDX-FileCopyrightText: 2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

function(libempp_validate_dependency_prefix dependency option_name)
	set(prefix "${${option_name}}")

	if (NOT prefix)
		return()
	endif ()

	if (NOT IS_ABSOLUTE "${prefix}")
		message(FATAL_ERROR
			"${PRO_NAME}: ${option_name} must be an absolute path."
		)
	endif ()

	if (NOT IS_DIRECTORY "${prefix}")
		message(FATAL_ERROR
			"${PRO_NAME}: ${dependency} install prefix does not exist: ${prefix}"
		)
	endif ()
endfunction()


function(libempp_validate_msvc_imported_targets package root_option)
	if (NOT MSVC)
		return()
	endif ()

	foreach(target IN LISTS ARGN)
		if (NOT TARGET ${target})
			continue()
		endif ()

		set(properties IMPORTED_LOCATION IMPORTED_IMPLIB)
		get_target_property(configurations ${target} IMPORTED_CONFIGURATIONS)

		foreach(configuration IN LISTS configurations)
			string(TOUPPER "${configuration}" configuration_upper)
			list(APPEND properties
				IMPORTED_LOCATION_${configuration_upper}
				IMPORTED_IMPLIB_${configuration_upper}
			)
		endforeach()

		foreach(property IN LISTS properties)
			get_target_property(paths ${target} ${property})

			if (NOT paths OR paths MATCHES "-NOTFOUND$")
				continue()
			endif ()

			foreach(path IN LISTS paths)
				string(TOLOWER "${path}" path_lower)

				if (path_lower MATCHES "mingw" OR path_lower MATCHES "\\.a$")
					message(FATAL_ERROR
						"${PRO_NAME}: ${package} target '${target}' resolves to "
						"'${path}', which is incompatible with MSVC. Install an "
						"MSVC-native ${package} package and set "
						"-D${root_option}=<prefix>."
					)
				endif ()
			endforeach()
		endforeach()
	endforeach()
endfunction()
