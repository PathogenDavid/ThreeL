#pragma once

// Slim down Windows headers
#define NOMINMAX
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP
#define NOMCX
#define NOSERVICE
#define NOHELP
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

#include <d3d12.h>
#include <dxcapi.h>
#include <d3d12shader.h>

#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <span>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "Assert.h"
#include "HashImplementations.h"
#include "Utilities.h"
