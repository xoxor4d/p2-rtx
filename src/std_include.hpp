#pragma once

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

// Version number
#include <version.hpp>

#include <windows.h>
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <mutex>
#include <filesystem>
#include <cassert>
#include <unordered_set>

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
#include "bridge_remix_api.h"

#include "utils/vector.hpp"
#include "game/structs.hpp"
#include "utils/hooking.hpp"
#include "utils/utils.hpp"
#include "utils/memory.hpp"
#include "utils/function.hpp"
#include "game/functions.hpp"

#include "components/loader.hpp"

using namespace std::literals;
