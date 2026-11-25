#pragma once

#include <node_api.h>

#include <string>

namespace NapiBinding {

std::string napiGetString(napi_env env, napi_value v);
napi_value fail(napi_env env, const char* message);

// rasterizePoints(positions: Float32Array, colors: Uint8Array | null, pngPath, width, height,
//                 azimuthDeg, elevationDeg, { fov, distScale, zUp }): boolean
napi_value RasterizePoints(napi_env env, napi_callback_info info);

napi_value Init(napi_env env, napi_value exports);

}
