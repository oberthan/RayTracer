#ifndef RENDERER_H
#define RENDERER_H
#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include <cmath>
#include "classes.h"

class Renderer {
public:
    Scene scene;

    explicit Renderer(Scene&& s) : scene(std::move(s)) {}

    void render() {
        int W = scene.camera.width;
        int H = scene.camera.height;

        SDL_Init(SDL_INIT_VIDEO);
        SDL_Window*   window  = SDL_CreateWindow("RayTracer", 1920, 1080, 0);
        SDL_Renderer* sdlRend = SDL_CreateRenderer(window, nullptr);
        SDL_Texture*  texture = SDL_CreateTexture(sdlRend,
                                    SDL_PIXELFORMAT_RGB24,
                                    SDL_TEXTUREACCESS_STREAMING, W, H);

        // Lock mouse to window for FPS-style look
        SDL_SetWindowRelativeMouseMode(window, true);

        std::vector<uint8_t> pixels(W * H * 3);
        float moveSpeed = 0.1f;
        float mouseSensitivity = 0.002f;
        bool running = true;

        while (running) {
            Uint64 frameStart = SDL_GetTicks();

            // Events
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) running = false;
                if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE)
                    running = false;

                // Mouse look
                if (e.type == SDL_EVENT_MOUSE_MOTION) {
                    scene.camera.yaw   -= e.motion.xrel * mouseSensitivity;
                    scene.camera.pitch -= e.motion.yrel * mouseSensitivity;
                    // Clamp pitch so you can't flip upside down
                    scene.camera.pitch = std::max(-1.5f, std::min(1.5f, scene.camera.pitch));
                    scene.camera.updateDirection();
                }
            }

            // WASD movement
            const bool* keys = SDL_GetKeyboardState(nullptr);
            Vec3& pos = scene.camera.origin;
            Vec3  dir = scene.camera.direction;
            Vec3  right = dir.cross(Vec3(0, 1, 0)).normalize();
            Vec3  up    = Vec3(0, 1, 0);

            if (keys[SDL_SCANCODE_W]) pos = pos + dir   * moveSpeed;
            if (keys[SDL_SCANCODE_S]) pos = pos - dir   * moveSpeed;
            if (keys[SDL_SCANCODE_D]) pos = pos + right * moveSpeed;
            if (keys[SDL_SCANCODE_A]) pos = pos - right * moveSpeed;
            if (keys[SDL_SCANCODE_E]) pos = pos + up    * moveSpeed;
            if (keys[SDL_SCANCODE_Q]) pos = pos - up    * moveSpeed;

            // Precompute camera basis vectors for ray direction
            Vec3 camDir   = scene.camera.direction;
            Vec3 camRight = camDir.cross(Vec3(0, 1, 0)).normalize();
            Vec3 camUp    = camRight.cross(camDir).normalize();

            #pragma omp parallel for schedule(dynamic, 4)
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    // Map pixel to camera space, then rotate into world space
                    float px = (float)(x - W / 2);
                    float py = (float)(H / 2 - y);
                    float pz = scene.camera.focal_length;

                    Vec3 worldDir = (camRight * px + camUp * py + camDir * pz).normalize();
                    Ray ray(scene.camera.origin, worldDir);
                    Color c = calculateLight(ray, 0, 3).clampOne();

                    int idx = (y * W + x) * 3;
                    pixels[idx + 0] = static_cast<uint8_t>(c.r * 255);
                    pixels[idx + 1] = static_cast<uint8_t>(c.g * 255);
                    pixels[idx + 2] = static_cast<uint8_t>(c.b * 255);
                }
            }

            SDL_UpdateTexture(texture, nullptr, pixels.data(), W * 3);
            SDL_RenderTexture(sdlRend, texture, nullptr, nullptr);

            Uint64 frameEnd = SDL_GetTicks();
            float fps = 1000.0f / (float)(frameEnd - frameStart + 1);
            SDL_SetWindowTitle(window, ("RayTracer | FPS: " + std::to_string((int)fps)).c_str());

            SDL_RenderPresent(sdlRend);
        }

        SDL_SetWindowRelativeMouseMode(window, false);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(sdlRend);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    HitResult traceRay(const Ray &ray) {
        HitResult closestHit = HitResult::NoHit;
        for (const auto &obj: scene.objects) {
            auto hr = obj->hit(ray);
            if (!hr.did_hit) continue;
            if (!closestHit.did_hit ||
                (hr.point - ray.origin).length() < (closestHit.point - ray.origin).length()) {
                closestHit = hr;
            }
        }
        return closestHit;
    }

    Color calculateLight(const Ray &ray, int depth, int maxDepth) {
        float t = std::max(0.0f, ray.direction.y);
        Color skyColor = Color(0.3f, 0.6f, 1.0f) * t;

        if (depth > maxDepth) return Color(1.0f, 1.0f,1.0f);

        const HitResult hit_result = traceRay(ray);
        if (!hit_result.did_hit) return skyColor;

        float diffuse = hit_result.material.diffuse;
        Vec3 normal   = hit_result.normal.normalize();

        // Ambient — always adds a base level of light
        float ambientStrength = 0.15f;
        Color lightColor = hit_result.material.color * ambientStrength;

        for (const auto &light: scene.lights) {
            Vec3 toLight    = light.origin - hit_result.point;
            Vec3 l          = toLight.normalize();
            float lightDist = toLight.length();

            // Shadow ray — offset origin slightly to avoid self-intersection
            Ray shadowRay(hit_result.point, l);
            HitResult shadowHit = traceRay(shadowRay);

            // Only shadowed if something is between surface and light
            if (shadowHit.did_hit &&
                (shadowHit.point - hit_result.point).length() < lightDist)
                continue;

            float attenuation = light.intensity / (lightDist * lightDist);

            float diff = std::max(0.0f, l * normal);

            Vec3 viewDir  = (hit_result.basePoint - hit_result.point).normalize();
            Vec3 halfway  = (l + viewDir).normalize();  // Blinn-Phong halfway vector
            float spec    = pow(std::max(0.0f, normal * halfway), hit_result.material.shininess);

            float d = diffuse * diff * attenuation;
            float s = (1.0f - diffuse) * spec * attenuation;

            lightColor = lightColor + light.color * (d + s);
        }

        // Reflection
        Vec3 reflectDir = ray.direction - normal * 2.0f * (ray.direction * normal);
        Ray  reflectRay(hit_result.point, reflectDir.normalize());
        Color reflectColor = calculateLight(reflectRay, depth + 1, maxDepth);

        Color objColor = hit_result.material.color;
        return objColor * lightColor + objColor * reflectColor * (1.0f - diffuse);
    }
};

#endif