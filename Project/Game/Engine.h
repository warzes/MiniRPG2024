#pragma once

//=============================================================================
// STL and 3rdparty Header
//=============================================================================

#if defined(_MSC_VER)
#	pragma warning(push, 3)
#endif

#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#endif // _WIN32
#if defined(__EMSCRIPTEN__)
#	include <emscripten/emscripten.h> // Emscripten functionality for C
#	include <emscripten/html5.h>      // Emscripten HTML5 library
#endif // __EMSCRIPTEN__

#include <glmConfig.h>
#include <glm/glm.hpp>

#if defined(_MSC_VER)
#	pragma warning(pop)
#endif

//=============================================================================
// Engine Header
//=============================================================================
