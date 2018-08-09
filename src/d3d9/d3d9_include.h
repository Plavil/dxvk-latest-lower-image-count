#pragma once

// Since we build on top of D3D11, we need to include that.
#include "../d3d11/d3d11_include.h"

// Main header for D3D9.
#include <d3d9.h>

// Validates a pointer parameter.
#define CHECK_NOT_NULL(ptr) { if (!(ptr)) { return D3DERR_INVALIDCALL; } }

// TODO: support D3D9 shared resources.
#define CHECK_SHARED_HANDLE(sh) { \
  if ((sh) != nullptr) { \
    Logger::err("D3D9 shared resources not yet supported"); \
    return D3DERR_INVALIDCALL; \
  } \
}
