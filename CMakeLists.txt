project(halo)
cmake_minimum_required(VERSION 3.16)

option(ENABLE_XDK_HYBRID "Compile select TUs with XDK MSVC 7.1 for matching FPU codegen" OFF)
option(HALO_TEST_HARNESS "Build with in-engine test harness enabled" OFF)

find_package(Python3 COMPONENTS Interpreter)

set(gen_dir "${PROJECT_BINARY_DIR}/generated")
add_custom_target(create_gen_dir ALL
	COMMAND ${CMAKE_COMMAND} -E make_directory ${gen_dir})

add_custom_command(
	OUTPUT "${gen_dir}/decl.h" "${gen_dir}/halo.xbe.def" "${gen_dir}/thunks.c"
	COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/analysis/knowledge.py"
		--gen-header "${gen_dir}/decl.h"
		--gen-def "${gen_dir}/halo.xbe.def"
		--gen-thunks "${gen_dir}/thunks.c"
	DEPENDS "${CMAKE_CURRENT_LIST_DIR}/tools/analysis/knowledge.py" "${CMAKE_CURRENT_LIST_DIR}/kb.json" create_gen_dir
	VERBATIM
)

if(MSVC)
	set(lib_tool "lib")
else()
	set(lib_tool "lld-link")
endif()

add_custom_command(
	OUTPUT "${gen_dir}/halo.xbe.lib"
	COMMAND ${lib_tool} /def:${gen_dir}/halo.xbe.def /machine:x86 /out:${gen_dir}/halo.xbe.lib
	DEPENDS "${gen_dir}/halo.xbe.def" create_gen_dir
	VERBATIM
)

add_custom_command(
	OUTPUT "${gen_dir}/xboxkrnl.exe.lib"
	COMMAND ${lib_tool} /def:${CMAKE_SOURCE_DIR}/src/xboxkrnl.exe.def /machine:x86 /out:${gen_dir}/xboxkrnl.exe.lib
	DEPENDS "${CMAKE_SOURCE_DIR}/src/xboxkrnl.exe.def" create_gen_dir
	VERBATIM
)

add_custom_command(
	OUTPUT "${gen_dir}/build_info.c" "${gen_dir}/.force"
	COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/build/gen-build-info.py" > ${gen_dir}/build_info.c
	DEPENDS "${CMAKE_CURRENT_LIST_DIR}/tools/build/gen-build-info.py" create_gen_dir
	VERBATIM
)

add_custom_command(
	OUTPUT "${gen_dir}/typechecks.c"
	COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/archive/gen-struct-checks.py" "${CMAKE_SOURCE_DIR}/src/types.h" > "${gen_dir}/typechecks.c"
	DEPENDS "${CMAKE_CURRENT_LIST_DIR}/tools/archive/gen-struct-checks.py" "${CMAKE_SOURCE_DIR}/src/types.h" create_gen_dir
	VERBATIM
)

add_custom_target(
	reverse_thunk_tests ALL
	COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/build/patch.py" --test-thunks
	DEPENDS
		"${CMAKE_CURRENT_LIST_DIR}/tools/build/patch.py"
		"${CMAKE_CURRENT_LIST_DIR}/tools/analysis/knowledge.py"
		"${CMAKE_CURRENT_LIST_DIR}/kb.json"
	VERBATIM
)

include_directories(${gen_dir} ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party/xbox)

if(CMAKE_BUILD_TYPE STREQUAL "")
	set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

if(MSVC)
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		message(FATAL_ERROR "You are attempting to build for x86-64 (64-bit). Delete the build directory, then specify -AWin32 when configuring with CMake to build for x86 (32-bit).")
	endif()
	set(CMAKE_C_FLAGS "/FI\"${CMAKE_SOURCE_DIR}/src/common.h\" /GS- /DMSVC")
	set(CMAKE_EXE_LINKER_FLAGS "/entry:_start /nodefaultlib:msvcrtd.lib /nodefaultlib:msvcrt.lib /incremental:no")
else()
	if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
		# FIXME: We can support other toolchains, but currently do not
		message(FATAL_ERROR "You are attempting to compile with ${CMAKE_CXX_COMPILER_ID}. Please use LLVM toolchain (clang, lld). Specify -DCMAKE_TOOLCHAIN_FILE=toolchains/llvm.cmake during configuration.")
	endif()
	set(CMAKE_C_FLAGS "-Wall -Werror -target i386-pc-win32 -march=pentium3 -mno-sse -nostdlib -ffreestanding -fno-builtin -fno-exceptions -I${CMAKE_SOURCE_DIR}/src -I${CMAKE_SOURCE_DIR}/third_party/xbox -include ${CMAKE_SOURCE_DIR}/src/common.h")
	set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld-link -Xlinker -entry:_start")

	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Og -gdwarf-4")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Xlinker -debug")
	endif()
endif()

if(ENABLE_XDK_HYBRID AND NOT MSVC)
	function(wsl_to_win PATH OUT_VAR)
		string(REGEX REPLACE "^/mnt/(.)" "\\1:" _result "${PATH}")
		set(${OUT_VAR} "${_result}" PARENT_SCOPE)
	endfunction()

	set(XDK_CL "/mnt/c/Program Files (x86)/RXDK/xbox/bin/vc71/CL.Exe")
	set(XDK_INC "C:/Program Files (x86)/RXDK/xbox/include")

	# TUs compiled with XDK MSVC 7.1 instead of clang.
	# Requirements: no inline asm (or asm guarded by #ifdef XDK_BUILD), C89 only.
	# Paths are relative to src/.
	set(XDK_HYBRID_SOURCES
		halo/cseries/cseries.c
		halo/effects/decals.c
		halo/effects/effects.c
		halo/game/game_engine.c
		halo/items/items.c
		halo/main/main.c
		halo/math/real_math.c
		halo/networking/network_server_manager.c
		halo/objects/objects.c
		halo/physics/point_physics.c
		halo/saved_games/game_state.c
		halo/sound/game_sound.c
		halo/sound/sound_manager.c
		halo/units/units.c
	)
endif()

add_executable(halo
		src/common.h)

add_subdirectory(src)

target_sources(halo PRIVATE
	${gen_dir}/thunks.c
	${gen_dir}/build_info.c
	${gen_dir}/typechecks.c
	)

if(HALO_TEST_HARNESS)
	target_compile_definitions(halo PRIVATE TEST_HARNESS=1)
	target_sources(halo PRIVATE src/halo/test_harness.c)
endif()

set(import_libs ${gen_dir}/halo.xbe.lib ${gen_dir}/xboxkrnl.exe.lib)
target_link_libraries(halo ${import_libs})
add_custom_target(import_libs_target DEPENDS ${import_libs})
add_dependencies(halo import_libs_target reverse_thunk_tests)

if(ENABLE_XDK_HYBRID AND NOT MSVC)
	set(_xdk_obj_dir "${CMAKE_BINARY_DIR}/xdk")
	file(MAKE_DIRECTORY "${_xdk_obj_dir}")

	wsl_to_win("${CMAKE_SOURCE_DIR}/src/xdk_common.h" _xdk_fi)
	wsl_to_win("${gen_dir}" _xdk_gen_inc)
	wsl_to_win("${CMAKE_SOURCE_DIR}/src" _xdk_src_inc)

	foreach(_src ${XDK_HYBRID_SOURCES})
		string(REPLACE "/" "_" _obj_name "${_src}")
		string(REPLACE ".c" ".obj" _obj_name "${_obj_name}")
		set(_obj "${_xdk_obj_dir}/${_obj_name}")

		wsl_to_win("${CMAKE_SOURCE_DIR}/src/${_src}" _src_win)
		wsl_to_win("${_obj}" _obj_win)

		add_custom_command(
			OUTPUT "${_obj}"
			COMMAND "${XDK_CL}"
				/nologo /c /TC
				/O2 /Oy- /GF /Gy /Gd
				/W0 /Zl /X
				/DMSVC /DXDK_BUILD
				"/FI${_xdk_fi}"
				"/I${_xdk_gen_inc}"
				"/I${_xdk_src_inc}"
				"/I${XDK_INC}"
				"/Fo${_obj_win}"
				"${_src_win}"
			DEPENDS
				"${CMAKE_SOURCE_DIR}/src/${_src}"
				"${CMAKE_SOURCE_DIR}/src/xdk_common.h"
				"${gen_dir}/decl.h"
			COMMENT "XDK: ${_src}"
			VERBATIM
		)

		target_sources(halo PRIVATE "${_obj}")
	endforeach()

	target_sources(halo PRIVATE "${CMAKE_SOURCE_DIR}/src/xdk_rt.c")
endif()

add_custom_command(TARGET halo POST_BUILD
	COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/audit/check_lift_hazards.py"
	COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/audit/check_raw_casts.py"
	COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/audit/check_asm_thunk_conflicts.py"
	VERBATIM
)

add_custom_target(
	patched_xbe ALL
	DEPENDS "$<TARGET_FILE:halo>" "${CMAKE_CURRENT_LIST_DIR}/tools/build/patch.py"
	BYPRODUCTS ${CMAKE_CURRENT_LIST_DIR}/halo-patched/default.xbe
	COMMAND
		"${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/audit/check_asm_thunk_conflicts.py"
	COMMAND
		"${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/audit/check_raw_casts.py"
	COMMAND
		"${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/audit/check_lift_hazards.py"
	COMMAND
		"${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/analysis/maintain.py" --check
	COMMAND
		"${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/analysis/kb_meta.py" validate
	COMMAND
		"${CMAKE_COMMAND}" -E env "PYTHONPATH=${CMAKE_CURRENT_LIST_DIR}"
			"${Python3_EXECUTABLE}" -m unittest
			tools.audit.test_patch.RegAnnotationBaselineTests
	COMMAND
		"${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/build/patch.py"
			"${CMAKE_CURRENT_LIST_DIR}/halo-patched/cachebeta.xbe"
			"$<TARGET_FILE:halo>"
			"${CMAKE_CURRENT_LIST_DIR}/halo-patched/default.xbe"
	VERBATIM
	)
