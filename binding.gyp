{
  "targets": [
    {
      "target_name": "headless_rasterizer",
      "sources": ["src/rasterizer.cpp"],
      "include_dirs": [],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "cflags": ["-O3", "-march=native", "-ffast-math", "-pthread"],
      "cflags_cc": ["-O3", "-march=native", "-ffast-math", "-std=c++17", "-pthread"],
      "ldflags": ["-pthread"],
      "defines": ["NAPI_CPP_EXCEPTIONS"]
    }
  ]
}
