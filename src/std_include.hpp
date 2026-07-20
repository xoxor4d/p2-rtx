#pragma once

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

constexpr auto COMP_MOD_VERSION_MAJOR = 2;
constexpr auto COMP_MOD_VERSION_MINOR = 3;
constexpr auto COMP_MOD_VERSION_PATCH = 1;

#define COMPMOD_ASSET_DIR "\\portal2-rtx\\"

// enable/disable benchmark logic
//#define BENCHMARK

#if defined(DEV_BUILD)
	inline constexpr bool IS_LATEST_BUILD = false;
#else
	inline constexpr bool IS_LATEST_BUILD = true;
#endif

#define USE_OFFSET(LATEST, OTHER) \
	(IS_LATEST_BUILD ? (LATEST) : (OTHER))

#define NOMINMAX
#include <windows.h>
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shellapi.h>
#include <chrono>
#include <mutex>
#include <filesystem>
#include <cassert>
#include <map>
#include <set>
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <xmmintrin.h>
#include <intrin.h>
#include <numbers>

#pragma warning(push)
#pragma warning(disable: 26495)
#include <d3d9.h>
#include <d3dx9.h>
#pragma warning(pop)

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)
#define AssertSize(x, size)								static_assert(sizeof(x) == size, STRINGIZE(x) " structure has an invalid size.")
#define STATIC_ASSERT_SIZE(struct, size)				static_assert(sizeof(struct) == size, "Size check")
#define STATIC_ASSERT_OFFSET(struct, member, offset)	static_assert(offsetof(struct, member) == offset, "Offset check")
#define XASSERT(x) if (x) MessageBoxA(HWND_DESKTOP, #x, "FATAL ERROR", MB_ICONERROR)

#include "MinHook.h"

#pragma warning(push)
#pragma warning(disable: 5311)
#include "toml.hpp"
#pragma warning(pop)
#include "bridge_remix_api.h"

#pragma warning(push)
#pragma warning(disable: 6011)
#pragma warning(disable: 28182)
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>
#include <misc/cpp/imgui_stdlib.h>
#pragma warning(pop)

#include "components/globals.hpp"
#include "game/structs.hpp"
#include "utils/utils.hpp"
#include "utils/vector.hpp"
#include "utils/hooking.hpp"
#include "utils/memory.hpp"
#include "utils/function.hpp"
#include "components/common/console.hpp"
#include "components/common/loader.hpp"

#include "sdk/engine/c_engine_client.hpp"
#include "sdk/vgui/surface/c_surface_mgr.hpp"
#include "sdk/inputsystem/c_input_stackstystem.hpp"
#include "sdk/entity/c_entity_list.hpp"

#include "game/functions.hpp"
#include "p2.hpp"

using namespace std::literals;
