#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include <cmath>
#include "classes.h"

// CUDA kernels and device code
__device__ float d_randomFloat(unsigned int *seed) {
    *seed = (*seed * 1103515245 + 12345) & 0x7fffffff;
    return (*seed / (float)0x7fffffff) * 0.5f;
}

__device__ Vec3 d_randomCosineHemisphere(const Vec3& normal, unsigned int *seed) {
    float u1 = d_randomFloat(seed);
    float u2 = d_randomFloat(seed);
    float r = sqrt(u1);
    float phi = 2.0f * PI * u2;
    float x = r * cos(phi);
    float z = r * sin(phi);
    float y = sqrt(fmax(0.0f, 1.0f - u1));

    Vec3 up = fabs(normal.x) > 0.9f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    Vec3 tangent = normal.cross(up);
    tangent = tangent * (1.0f / tangent.length());
    Vec3 binormal = normal.cross(tangent);

    return (tangent * x + normal * y + binormal * z) * (1.0f / (tangent * x + normal * y + binormal * z).length());
}

__device__ Vec3 d_ggxSample(const Vec3 &normal, float roughness, unsigned int *seed) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;

    float xi1 = d_randomFloat(seed);
    float xi2 = d_randomFloat(seed);

    float theta = atan(alpha * sqrt(xi1 / fmax(1e-6f, 1.0f - xi1)));
    float phi = 2.0f * PI * xi2;

    float sinTheta = sin(theta);
    float cosTheta = cos(theta);
    Vec3 h(sinTheta * cos(phi), cosTheta, sinTheta * sin(phi));

    Vec3 up = fabs(normal.x) > 0.9f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    Vec3 tangent = normal.cross(up);
    tangent = tangent * (1.0f / tangent.length());
    Vec3 binormal = normal.cross(tangent);

    return (tangent * h.x + normal * h.y + binormal * h.z) * (1.0f / (tangent * h.x + normal * h.y + binormal * h.z).length());
}

__device__ float d_ggxNDF(float NdotH, float alpha2) {
    float d = (NdotH * NdotH * (alpha2 - 1.0f) + 1.0f);
    return alpha2 / (PI * d * d);
}

__device__ float d_smithG_sub(float NdotV, float alpha) {
    float k = alpha / 2.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

__device__ float d_smithG(float NdotV, float NdotL, float alpha) {
    return d_smithG_sub(NdotV, alpha) * d_smithG_sub(NdotL, alpha);
}

__device__ Color d_schlickFresnel(float VdotH, const Color& F0) {
    float f = pow(1.0f - fmax(0.0f, VdotH), 5.0f);
    return F0 + (Color(1, 1, 1) - F0) * f;
}

// GPU data structures
struct GPUObject {
    Vec3 origin;
    float radius;
    Material material;
    int type; // 0 = sphere, 1 = rectangle
};

struct GPURectangle {
    Vec3 origin, aV, bV, nV;
    Material material;
};

__constant__ int d_maxObjects = 256;
__constant__ int d_maxRectangles = 256;

__device__ HitResult d_traceRay(const Ray &ray, GPUObject *objects, int numObjects, 
                                  GPURectangle *rectangles, int numRectangles) {
    HitResult closestHit = HitResult::NoHit;

    // Check spheres
    for (int i = 0; i < numObjects; i++) {
        if (objects[i].type != 0) continue;

        Vec3 O = ray.origin - objects[i].origin;
        float a = ray.direction * ray.direction;
        float b = O * ray.direction * 2;
        float c = O * O - objects[i].radius * objects[i].radius;
        float disc = b * b - 4 * a * c;

        if (disc > 0) {
            float t = (-b - sqrt(disc)) / (2 * a);
            if (t > 0) {
                Vec3 p = ray.origin + ray.direction * t;
                Vec3 n = (p - objects[i].origin) * (1.0f / (p - objects[i].origin).length());
                HitResult hr(p, ray.origin, n, objects[i].material, 0, 0);
                
                if (!closestHit.did_hit || (hr.point - ray.origin).length() < (closestHit.point - ray.origin).length()) {
                    closestHit = hr;
                }
            }
        }
    }

    // Check rectangles
    for (int i = 0; i < numRectangles; i++) {
        Vec3 pV = ray.direction.cross(rectangles[i].bV);
        float det = rectangles[i].aV * pV;
        if (det <= 0) continue;

        float invDet = 1.0f / det;
        Vec3 tV = ray.origin - rectangles[i].origin;
        float u = (tV * pV) * invDet;
        if (u < 0 || u > 1) continue;

        Vec3 qV = tV.cross(rectangles[i].aV);
        float v = (ray.direction * qV) * invDet;
        if (v < 0 || v > 1) continue;

        float t = (rectangles[i].bV * qV) * invDet;
        if (t < 0) continue;

        Vec3 intersectionPoint = ray.origin + ray.direction * t;
        HitResult hr(intersectionPoint, ray.origin, rectangles[i].nV, rectangles[i].material, u, v);
        
        if (!closestHit.did_hit || (hr.point - ray.origin).length() < (closestHit.point - ray.origin).length()) {
            closestHit = hr;
        }
    }

    return closestHit;
}

__device__ Color d_tracePath(const Ray &ray, int depth, int maxDepth, 
                              GPUObject *objects, int numObjects,
                              GPURectangle *rectangles, int numRectangles,
                              unsigned int *seed) {
    auto skyColor = [](const Vec3 &dir) {
        return Color(0.01f, 0.01f, 0.01f);
    };

    if (depth >= maxDepth) return skyColor(ray.direction);

    HitResult hit = d_traceRay(ray, objects, numObjects, rectangles, numRectangles);
    if (!hit.did_hit) return skyColor(ray.direction);

    if (hit.material.emissionStrength > 0.0f)
        return hit.material.emission * hit.material.emissionStrength;

    Vec3 normal = hit.normal * (1.0f / hit.normal.length());
    if (normal * ray.direction > 0) normal = -normal;

    Vec3 v = (-ray.direction) * (1.0f / (-ray.direction).length());
    float rough = fmax(0.001f, hit.material.roughness);
    float alpha = rough * rough;
    float alpha2 = alpha * alpha;

    Color F0 = Color(0.04f, 0.04f, 0.04f) * (1.0f - hit.material.metallic)
               + hit.material.color * hit.material.metallic;

    float F0avg = (F0.r + F0.g + F0.b) / 3.0f;
    float NdotV = fmax(0.0f, normal * v);
    float fresnel = F0avg + (1.0f - F0avg) * pow(1.0f - NdotV, 5.0f);
    float specProb = fmax(0.1f, fmin(0.9f, fresnel));

    Vec3 bounceDir;
    float pdf;
    Color brdf;

    if (d_randomFloat(seed) < specProb) {
        Vec3 h = d_ggxSample(normal, rough, seed);
        bounceDir = (ray.direction - h * 2.0f * (ray.direction * h)) * 
                    (1.0f / (ray.direction - h * 2.0f * (ray.direction * h)).length());

        if (normal * bounceDir <= 0.0f) return Color(0, 0, 0);

        float NdotH = fmax(0.0f, normal * h);
        float NdotL = fmax(0.0f, normal * bounceDir);
        float VdotH = fmax(0.0f, v * h);

        float D = d_ggxNDF(NdotH, alpha2);
        Color F = d_schlickFresnel(VdotH, F0);
        float G = d_smithG(NdotV, NdotL, alpha);

        float denom = fmax(1e-6f, 4.0f * NdotV * NdotL);
        brdf = F * (D * G / denom);

        pdf = (D * NdotH) / fmax(1e-6f, 4.0f * VdotH);
        pdf *= specProb;
    } else {
        bounceDir = d_randomCosineHemisphere(normal, seed);

        float NdotL = fmax(0.0f, normal * bounceDir);
        float VdotH = NdotV;
        Color F = d_schlickFresnel(VdotH, F0);

        brdf = (Color(1, 1, 1) - F) * hit.material.color
               * (1.0f / PI)
               * (1.0f - hit.material.metallic);

        pdf = fmax(0.0f, normal * bounceDir) / PI;
        pdf *= (1.0f - specProb);
    }

    float NdotL = fmax(0.0f, normal * bounceDir);
    if (pdf < 1e-6f) return Color(0, 0, 0);

    Color directLight(0, 0, 0);
    for (int i = 0; i < numObjects; i++) {
        if (objects[i].material.emissionStrength <= 0.0f) continue;
        if (objects[i].type != 0) continue;

        Vec3 lightSampleDir = d_randomCosineHemisphere(
            (objects[i].origin - hit.point) * (1.0f / (objects[i].origin - hit.point).length()), seed);
        Vec3 lightPoint = objects[i].origin + lightSampleDir * objects[i].radius;

        Vec3 toLight = lightPoint - hit.point;
        float lightDist = toLight.length();
        Vec3 l = toLight * (1.0f / lightDist);

        float NdotL_direct = normal * l;
        if (NdotL_direct <= 0.0f) continue;

        Ray shadowRay(hit.point + normal * 0.001f, l);
        HitResult shadowHit = d_traceRay(shadowRay, objects, numObjects, rectangles, numRectangles);

        bool blocked = shadowHit.did_hit &&
                       (shadowHit.point - hit.point).length() < lightDist - 0.01f;
        if (blocked) continue;

        float attenuation = 1.0f / (lightDist * lightDist);
        Color emission = objects[i].material.emission * objects[i].material.emissionStrength;
        directLight = directLight + hit.material.color * emission
                      * (NdotL_direct * attenuation);
    }

    Ray bounceRay(hit.point + normal * 0.001f, bounceDir);
    Color incoming = d_tracePath(bounceRay, depth + 1, maxDepth, objects, numObjects, rectangles, numRectangles, seed);

    return directLight + brdf * incoming * (NdotL / pdf);
}

__global__ void raytraceKernel(Color *accumulator, int W, int H, 
                                Vec3 camOrigin, Vec3 camDir, Vec3 camRight, Vec3 camUp, float focalLength,
                                GPUObject *objects, int numObjects,
                                GPURectangle *rectangles, int numRectangles,
                                int sampleCount) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= W || y >= H) return;

    unsigned int seed = (y * W + x) * (sampleCount + 1);

    float jx = d_randomFloat(&seed) - 0.5f;
    float jy = d_randomFloat(&seed) - 0.5f;

    float px = (float)(x - W / 2);
    float py = (float)(H / 2 - y);

    Vec3 worldDir = (camRight * px + camUp * py + camDir * focalLength);
    worldDir = worldDir * (1.0f / worldDir.length());

    Ray ray(camOrigin, worldDir);

    Color sample = d_tracePath(ray, 0, 8, objects, numObjects, rectangles, numRectangles, &seed);
    accumulator[y * W + x] += sample;
}

class Renderer {
public:
    Scene scene;
    GPUObject *d_objects;
    GPURectangle *d_rectangles;
    Color *d_accumulator;
    int numObjects;
    int numRectangles;

    explicit Renderer(Scene&& s) : scene(std::move(s)), numObjects(0), numRectangles(0) {
        // Copy scene objects to GPU
        std::vector<GPUObject> gpuObjects;
        std::vector<GPURectangle> gpuRectangles;

        for (const auto &obj : scene.objects) {
            if (auto *sphere = dynamic_cast<Sphere *>(obj)) {
                GPUObject go;
                go.origin = sphere->origin;
                go.radius = sphere->radius;
                go.material = sphere->material;
                go.type = 0;
                gpuObjects.push_back(go);
            } else if (auto *rect = dynamic_cast<Rectangle *>(obj)) {
                GPURectangle gr;
                gr.origin = rect->origin;
                gr.aV = rect->aV;
                gr.bV = rect->bV;
                gr.nV = rect->nV;
                gr.material = rect->material;
                gpuRectangles.push_back(gr);
            }
        }

        numObjects = gpuObjects.size();
        numRectangles = gpuRectangles.size();

        cudaMalloc(&d_objects, numObjects * sizeof(GPUObject));
        cudaMalloc(&d_rectangles, numRectangles * sizeof(GPURectangle));

        if (numObjects > 0)
            cudaMemcpy(d_objects, gpuObjects.data(), numObjects * sizeof(GPUObject), cudaMemcpyHostToDevice);
        if (numRectangles > 0)
            cudaMemcpy(d_rectangles, gpuRectangles.data(), numRectangles * sizeof(GPURectangle), cudaMemcpyHostToDevice);
    }

    ~Renderer() {
        cudaFree(d_objects);
        cudaFree(d_rectangles);
        cudaFree(d_accumulator);
    }

    void render() {
        int W = scene.camera.width;
        int H = scene.camera.height;
        int displayW = 1920, displayH = 1080;

        SDL_Init(SDL_INIT_VIDEO);
        SDL_Window *window = SDL_CreateWindow("RayTracer", displayW, displayH, 0);
        SDL_Renderer *sdlRend = SDL_CreateRenderer(window, nullptr);
        SDL_Texture *texture = SDL_CreateTexture(sdlRend,
                                                 SDL_PIXELFORMAT_RGB24,
                                                 SDL_TEXTUREACCESS_STREAMING, W, H);

        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
        SDL_SetWindowRelativeMouseMode(window, true);

        std::vector<Color> h_accumulator(W * H, Color(0, 0, 0));
        cudaMalloc(&d_accumulator, W * H * sizeof(Color));
        cudaMemcpy(d_accumulator, h_accumulator.data(), W * H * sizeof(Color), cudaMemcpyHostToDevice);

        std::vector<uint8_t> pixels(W * H * 3);

        float moveSpeed = 0.1f;
        float mouseSensitivity = 0.002f;
        int sampleCount = 0;
        bool running = true;
        bool cameraMoved = false;

        while (running) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) running = false;
                if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE)
                    running = false;

                if (e.type == SDL_EVENT_MOUSE_MOTION) {
                    scene.camera.yaw -= e.motion.xrel * mouseSensitivity;
                    scene.camera.pitch -= e.motion.yrel * mouseSensitivity;
                    scene.camera.pitch = std::max(-1.5f, std::min(1.5f, scene.camera.pitch));
                    scene.camera.updateDirection();
                    cameraMoved = true;
                }
            }

            const bool *keys = SDL_GetKeyboardState(nullptr);
            Vec3 &pos = scene.camera.origin;
            Vec3 dir = scene.camera.direction;
            Vec3 right = dir.cross(Vec3(0, 1, 0)) * (1.0f / dir.cross(Vec3(0, 1, 0)).length());
            Vec3 up = Vec3(0, 1, 0);

            if (keys[SDL_SCANCODE_W]) pos = pos + dir * moveSpeed;
            if (keys[SDL_SCANCODE_S]) pos = pos - dir * moveSpeed;
            if (keys[SDL_SCANCODE_D]) pos = pos + right * moveSpeed;
            if (keys[SDL_SCANCODE_A]) pos = pos - right * moveSpeed;
            if (keys[SDL_SCANCODE_E]) pos = pos + up * moveSpeed;
            if (keys[SDL_SCANCODE_Q]) pos = pos - up * moveSpeed;

            if (cameraMoved) {
                std::fill(h_accumulator.begin(), h_accumulator.end(), Color(0, 0, 0));
                cudaMemcpy(d_accumulator, h_accumulator.data(), W * H * sizeof(Color), cudaMemcpyHostToDevice);
                sampleCount = 0;
                cameraMoved = false;
            }

            sampleCount++;

            Vec3 camDir = scene.camera.direction;
            Vec3 camRight = camDir.cross(Vec3(0, 1, 0)) * (1.0f / camDir.cross(Vec3(0, 1, 0)).length());
            Vec3 camUp = camRight.cross(camDir) * (1.0f / camRight.cross(camDir).length());
            float pz = scene.camera.focal_length;

            dim3 blockSize(16, 16);
            dim3 gridSize((W + blockSize.x - 1) / blockSize.x, (H + blockSize.y - 1) / blockSize.y);

            raytraceKernel<<<gridSize, blockSize>>>(d_accumulator, W, H,
                                                     camDir, camDir, camRight, camUp, pz,
                                                     d_objects, numObjects,
                                                     d_rectangles, numRectangles,
                                                     sampleCount);

            cudaMemcpy(h_accumulator.data(), d_accumulator, W * H * sizeof(Color), cudaMemcpyDeviceToHost);

            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    Color avg = h_accumulator[y * W + x] * (1.0f / sampleCount);
                    Color display = avg.reinhardTonemap().gammaCorrect().clampOne();

                    int idx = (y * W + x) * 3;
                    pixels[idx + 0] = static_cast<uint8_t>(display.r * 255);
                    pixels[idx + 1] = static_cast<uint8_t>(display.g * 255);
                    pixels[idx + 2] = static_cast<uint8_t>(display.b * 255);
                }
            }

            SDL_UpdateTexture(texture, nullptr, pixels.data(), W * 3);
            SDL_RenderTexture(sdlRend, texture, nullptr, nullptr);
            SDL_SetWindowTitle(window, ("RayTracer | Samples: " +
                                        std::to_string(sampleCount)).c_str());
            SDL_RenderPresent(sdlRend);
        }

        SDL_SetWindowRelativeMouseMode(window, false);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(sdlRend);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

#endif