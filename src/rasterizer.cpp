#include <node_api.h>
#include <string>
#include "rasterizer/rasterizer.h"

namespace NapiBinding {

static std::string napiGetString(napi_env env, napi_value v) {
    size_t len = 0;
    napi_get_value_string_utf8(env, v, nullptr, 0, &len);
    std::string s(len, '\0');
    napi_get_value_string_utf8(env, v, &s[0], len + 1, &len);
    return s;
}

static napi_value fail(napi_env env, const char* message) {
    napi_throw_type_error(env, nullptr, message);
    return nullptr;
}

// rasterizePoints(positions: Float32Array, colors: Uint8Array | null, pngPath, width, height,
//                 azimuthDeg, elevationDeg, { fov, distScale, zUp }): boolean
static napi_value RasterizePoints(napi_env env, napi_callback_info info) {
    size_t argc = 8;
    napi_value args[8];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if(argc < 8) return fail(env, "rasterizePoints: expected 8 arguments");

    bool isTyped = false;
    napi_typedarray_type type;
    size_t length = 0;
    void* data = nullptr;
    napi_is_typedarray(env, args[0], &isTyped);
    if(!isTyped) return fail(env, "rasterizePoints: positions must be a Float32Array");
    napi_get_typedarray_info(env, args[0], &type, &length, &data, nullptr, nullptr);
    if(type != napi_float32_array || length % 3 != 0) return fail(env, "rasterizePoints: positions must be a Float32Array of xyz triples");
    const float* positions = (const float*)data;
    const size_t count = length / 3;

    const uint8_t* colors = nullptr;
    napi_valuetype colorsType;
    napi_typeof(env, args[1], &colorsType);
    if(colorsType != napi_null && colorsType != napi_undefined){
        napi_is_typedarray(env, args[1], &isTyped);
        if(!isTyped) return fail(env, "rasterizePoints: colors must be a Uint8Array of RGBA8 or null");
        napi_get_typedarray_info(env, args[1], &type, &length, &data, nullptr, nullptr);
        if(type != napi_uint8_array || length != count * 4) return fail(env, "rasterizePoints: colors must hold RGBA8 for every point");
        colors = (const uint8_t*)data;
    }

    std::string pngPath = napiGetString(env, args[2]);
    int32_t width = 0, height = 0;
    napi_get_value_int32(env, args[3], &width);
    napi_get_value_int32(env, args[4], &height);
    double az = 0, el = 0;
    napi_get_value_double(env, args[5], &az);
    napi_get_value_double(env, args[6], &el);

    napi_value opts = args[7], v;
    Rasterizer::Options ropts{};
    double fov = ropts.fovDeg, distScale = ropts.distScale;
    bool zUp = ropts.zUp;
    if(napi_get_named_property(env, opts, "fov", &v) == napi_ok) napi_get_value_double(env, v, &fov);
    if(napi_get_named_property(env, opts, "distScale", &v) == napi_ok) napi_get_value_double(env, v, &distScale);
    if(napi_get_named_property(env, opts, "zUp", &v) == napi_ok) napi_get_value_bool(env, v, &zUp);
    ropts.fovDeg = (float)fov;
    ropts.distScale = (float)distScale;
    ropts.zUp = zUp;

    Rasterizer raster;
    bool ok = raster.rasterizePoints(positions, colors, count, pngPath.c_str(), width, height, (float)az, (float)el, ropts);

    napi_value res;
    napi_get_boolean(env, ok, &res);
    return res;
}

static napi_value Init(napi_env env, napi_value exports) {
    napi_value fn;
    napi_create_function(env, nullptr, 0, RasterizePoints, nullptr, &fn);
    napi_set_named_property(env, exports, "rasterizePoints", fn);
    return exports;
}

}

NAPI_MODULE(NODE_GYP_MODULE_NAME, NapiBinding::Init)
