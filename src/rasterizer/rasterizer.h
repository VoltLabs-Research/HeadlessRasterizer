/**
 * Copyright(c) 2025, Rodolfo Herrera Hernandez. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#pragma once

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <malloc.h>
#endif

/**
 * Renders a point cloud (xyz floats + RGBA8 colours) to a PNG with a z-buffered splat per point.
 * The caller decodes the model; this class only draws, so it carries no knowledge of file formats.
 */
class Rasterizer{
public:
    struct Options{
        // Vertical field of view in degrees.
        float fovDeg = 60.f;
        // Multiplicative scale factor applied to camera distance.
        float distScale = 1.f;
        // If true: Z is up; otherwise Y is up.
        bool zUp = true;
    };

    bool rasterizePoints(
        const float* pos,
        const uint8_t* rgba,
        size_t count,
        const char* pngPath,
        int width,
        int height,
        float azDeg,
        float elDeg,
        const Options& opts
    ){
        if(!pos || count == 0 || width <= 0 || height <= 0) return false;

        Bounds b = computeBoundsParallel(pos, count);
        float cx = (b.minX + b.maxX) * 0.5f;
        float cy = (b.minY + b.maxY) * 0.5f;
        float cz = (b.minZ + b.maxZ) * 0.5f;

        float dx = b.maxX - b.minX, dy = b.maxY - b.minY, dz = b.maxZ - b.minZ;
        float radius = 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);

        float fovRad = opts.fovDeg * 3.1415926535f / 180.f;
        float aspect = (float)width / height;

        float distance = 1.2f * radius / std::tan(fovRad * 0.5f) * opts.distScale;
        if(distance < 1e-3f) distance = 1e-3f;

        float znear = std::max(1e-3f, distance - radius * 2.f);
        float zfar  = distance + radius * 2.f;

        float az = azDeg * 3.1415926535f / 180.f;
        float el = elDeg * 3.1415926535f / 180.f;

        float dirX, dirY, dirZ;
        if(opts.zUp){
            dirX = std::cos(el) * std::cos(az);
            dirY = std::cos(el) * std::sin(az);
            dirZ = std::sin(el);
        }else{
            dirX = std::cos(el) * std::cos(az);
            dirY = std::sin(el);
            dirZ = std::cos(el) * std::sin(az);
        }

        Mat4 view = Mat4::lookAt(
            cx + dirX * distance, cy + dirY * distance, cz + dirZ * distance,
            cx, cy, cz,
            0.f, opts.zUp ? 0.f : 1.f, opts.zUp ? 1.f : 0.f
        );
        Mat4 proj = Mat4::perspective(fovRad, aspect, znear, zfar);
        Mat4 mvp = proj * view;

        size_t pixels = (size_t)width * (size_t)height;
        std::vector<uint32_t> zBuffer(pixels, 0xFFFFFFFF);
        std::vector<uint8_t> colorBuffer(pixels * 4, 0);

        splat(pos, rgba, count, mvp, width, height, zBuffer.data(), colorBuffer.data());

        return stbi_write_png(pngPath, width, height, 4, colorBuffer.data(), width * 4) != 0;
    }

private:
    struct Mat4{
        float m[16];

        static Mat4 lookAt(
            float eyeX,
            float eyeY,
            float eyeZ,
            float cx,
            float cy,
            float cz,
            float upX,
            float upY,
            float upZ
        ){
            float fx = cx - eyeX;
            float fy = cy - eyeY;
            float fz = cz - eyeZ;
            
            float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
            if(fl < 1e-20f) fl = 1.f;

            fx /= fl;
            fy /= fl;
            fz /= fl;

            float sx = fy * upZ - fz * upY;
            float sy = fz * upX - fx * upZ;
            float sz = fx * upY - fy * upX;
            
            float sl = std::sqrt(sx * sx + sy * sy + sz * sz);
            if(sl < 1e-20f) sl = 1.f;

            sx /= sl;
            sy /= sl;
            sz /= sl;

            float ux = sy * fz - sz * fy;
            float uy = sz * fx - sx * fz;
            float uz = sx * fy - sy * fx;

            Mat4 r{};
            r.m[0] = sx;
            r.m[4] = sy;
            r.m[8] = sz;
            r.m[12] = -(sx * eyeX + sy * eyeY + sz * eyeZ);

            r.m[1] = ux; 
            r.m[5] = uy;
            r.m[9] = uz;
            r.m[13] = -(ux * eyeX + uy * eyeY + uz * eyeZ);

            r.m[2] = -fx;
            r.m[6] = -fy;
            r.m[10] = -fz; 
            r.m[14] = (fx * eyeX + fy * eyeY + fz * eyeZ);

            r.m[3] = 0.f;
            r.m[7] = 0.f; 
            r.m[11] = 0.f;
            r.m[15] = 1.f;

            return r;
        }

        static Mat4 perspective(float fovRad, float aspect, float znear, float zfar){
            float f = 1.0f / std::tan(fovRad * 0.5f);
            Mat4 r{};
            r.m[0] = f / aspect;
            r.m[5] = f;
            r.m[10] = (zfar + znear) / (znear - zfar);
            r.m[11] = -1.f;
            r.m[14] = (2.f * zfar * znear) / (znear - zfar);
            return r;
        }

        Mat4 operator*(const Mat4& b) const{
            Mat4 r{};
            for(int i = 0; i < 4; i++){
                const float a0 = m[i];
                const float a1 = m[4 + i];
                const float a2 = m[8 + i];
                const float a3 = m[12 + i];

                r.m[0 * 4 + i] = a0 * b.m[0] + a1 * b.m[1] + a2 * b.m[2] + a3 * b.m[3];
                r.m[1 * 4 + i] = a0 * b.m[4] + a1 * b.m[5] + a2 * b.m[6] + a3 * b.m[7];
                r.m[2 * 4 + i] = a0 * b.m[8] + a1 * b.m[9] + a2 * b.m[10] + a3 * b.m[11];
                r.m[3 * 4 + i] = a0 * b.m[12] + a1 * b.m[13] + a2 * b.m[14] + a3 * b.m[15];
            }
            return r;
        }
    };

    struct Vec4{
        float x, y, z, w;
    };

    static inline Vec4 project4(const Mat4& mvp, float x, float y, float z){
        Vec4 r;
        r.x = mvp.m[0] * x + mvp.m[4] * y + mvp.m[8] * z + mvp.m[12];
        r.y = mvp.m[1] * x + mvp.m[5] * y + mvp.m[9] * z + mvp.m[13];
        r.z = mvp.m[2] * x + mvp.m[6] * y + mvp.m[10] * z + mvp.m[14];
        r.w = mvp.m[3] * x + mvp.m[7] * y + mvp.m[11] * z + mvp.m[15];
        return r;
    }

    struct Bounds{
        float minX, minY, minZ;
        float maxX, maxY, maxZ;
    };

    static Bounds computeBoundsParallel(const float* pos, size_t n){
        unsigned T = std::min(16u, std::max(1u, std::thread::hardware_concurrency()));
        std::vector<Bounds> b(T);
        size_t block = (n + T - 1) / T;
        
        std::vector<std::thread> threads;
        for(unsigned t = 0; t < T; t++){
            size_t start = t * block;
            size_t end = std::min(start + block, n);
            threads.emplace_back([&, t, start, end](){
                Bounds bb = {1e30f, 1e30f, 1e30f, -1e30f, -1e30f, -1e30f};
                for(size_t i = start; i < end; i++){
                    float x = pos[i * 3], y = pos[i * 3 + 1], z = pos[i * 3 + 2];
                    if(x < bb.minX) bb.minX = x;
                    if(x > bb.maxX) bb.maxX = x;

                    if(y < bb.minY) bb.minY = y; 
                    if(y > bb.maxY) bb.maxY = y;

                    if(z < bb.minZ) bb.minZ = z;
                    if(z > bb.maxZ) bb.maxZ = z;
                }
                b[t] = bb;
            });
        }

        for(auto &a : threads) a.join();

        Bounds out = b[0];
        for(unsigned t = 1; t < T; t++){
            out.minX = std::min(out.minX, b[t].minX); 
            out.maxX = std::max(out.maxX, b[t].maxX);

            out.minY = std::min(out.minY, b[t].minY);
            out.maxY = std::max(out.maxY, b[t].maxY);

            out.minZ = std::min(out.minZ, b[t].minZ);
            out.maxZ = std::max(out.maxZ, b[t].maxZ);
        }
        return out;
    }

    static void splat(
        const float* pos,
        const uint8_t* rgba,
        size_t n,
        const Mat4& mvp,
        int width,
        int height,
        uint32_t* zBuffer,
        uint8_t* colorBuffer
    ){
        // Adaptive point radius based on density
        float density = (float) n / ((float) width * height);
        int pointRadius = density <= 0.1f ? 5 : (density <= 0.5f ? 4 : (density <= 1.5f ? 3 : 2));
        int r2 = pointRadius * pointRadius;

        // Keep z/color writes ordered to avoid non-deterministic buffer races.
        for(size_t i = 0; i < n; i++){
            Vec4 vec = project4(mvp, pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]);
            if(vec.w <= 1e-6f) continue;

            float invW = 1.f / vec.w;
            float ndcX = vec.x * invW, ndcY = vec.y * invW, ndcZ = vec.z * invW;
            if(ndcZ < -1.f || ndcZ > 1.f) continue;

            int ix = (int)((ndcX * 0.5f + 0.5f) * width);
            int iy = (int)((1.f - (ndcY * 0.5f + 0.5f)) * height);
            if(ix < -pointRadius || ix >= width + pointRadius ||
                iy < -pointRadius || iy >= height + pointRadius) continue;

            uint32_t z24 = (uint32_t)((ndcZ + 1.f) * 0.5f * 16777215.f);
            uint8_t R = rgba ? rgba[i * 4] : 180;
            uint8_t G = rgba ? rgba[i * 4 + 1] : 180;
            uint8_t B = rgba ? rgba[i * 4 + 2] : 180;

            for(int dy = -pointRadius; dy <= pointRadius; dy++){
                int yy = iy + dy;
                if(yy < 0 || yy >= height) continue;

                for(int dx = -pointRadius; dx <= pointRadius; dx++){
                    int xx = ix + dx;
                    if(xx < 0 || xx >= width) continue;
                    if(dx * dx + dy * dy > r2) continue;

                    size_t idx = (size_t)yy * width + xx;
                    if(z24 < zBuffer[idx]){
                        zBuffer[idx] = z24;
                        colorBuffer[idx * 4] = R;
                        colorBuffer[idx * 4 + 1] = G;
                        colorBuffer[idx * 4 + 2] = B;
                        colorBuffer[idx * 4 + 3] = 255;
                    }
                }
            }
        }
    }
};
