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

    explicit Renderer(Scene &&s) : scene(std::move(s)) {
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

        // Per-pixel state
        std::vector<Color> accumulator(W * H, Color(0, 0, 0));
        std::vector<Color> accumSq(W * H, Color(0, 0, 0));
        std::vector<float> semPerPixel(W * H, 1e6f); // current SEM estimate
        std::vector<int> samplesPer(W * H, 0);
        std::vector<bool> converged(W * H, false);

        const int minSamples = 64;
        const float semThreshold = 0.01f; // converged when SEM < this
        const int budgetPerFrame = 4; // max extra samples for noisy pixels
        std::vector<uint8_t> pixels(W * H * 3);

        float moveSpeed = 0.1f;
        float mouseSensitivity = 0.002f;
        int sampleCount = 0;
        bool running = true;
        bool cameraMoved = false;

        while (running) {
            // Events
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) running = false;
                if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE)
                    running = false;

                // Mouse look
                if (e.type == SDL_EVENT_MOUSE_MOTION) {
                    scene.camera.yaw -= e.motion.xrel * mouseSensitivity;
                    scene.camera.pitch -= e.motion.yrel * mouseSensitivity;
                    // Clamp pitch so you can't flip upside down
                    scene.camera.pitch = std::max(-1.5f, std::min(1.5f, scene.camera.pitch));
                    scene.camera.updateDirection();
                    cameraMoved = true;
                }
            }

            // WASD movement
            const bool *keys = SDL_GetKeyboardState(nullptr);
            Vec3 &pos = scene.camera.origin;
            Vec3 dir = scene.camera.direction;
            Vec3 right = dir.cross(Vec3(0, 1, 0)).normalize();

            Vec3 up = Vec3(0, 1, 0);
            if (keys[SDL_SCANCODE_W]) pos = pos + dir * moveSpeed;
            if (keys[SDL_SCANCODE_S]) pos = pos - dir * moveSpeed;
            if (keys[SDL_SCANCODE_D]) pos = pos + right * moveSpeed;
            if (keys[SDL_SCANCODE_A]) pos = pos - right * moveSpeed;
            if (keys[SDL_SCANCODE_E]) pos = pos + up * moveSpeed;
            if (keys[SDL_SCANCODE_Q]) pos = pos - up * moveSpeed;

            if (cameraMoved) {
                std::fill(accumulator.begin(), accumulator.end(), Color(0, 0, 0));
                std::fill(accumSq.begin(), accumSq.end(), Color(0, 0, 0));
                std::fill(converged.begin(), converged.end(), false);
                std::fill(samplesPer.begin(), samplesPer.end(), 0);
                sampleCount = 0;
                cameraMoved = false;
            }

            sampleCount++;

            // Precompute camera basis vectors for ray direction
            Vec3 camDir = scene.camera.direction;
            Vec3 camRight = camDir.cross(Vec3(0, 1, 0)).normalize();
            Vec3 camUp = camRight.cross(camDir).normalize();
            float pz = scene.camera.focal_length;

            #pragma omp parallel for schedule(dynamic, 4)
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    int i = y * W + x;

                    if (samplesPer[i] == 0) {
                        float px = (x - W / 2.0f);
                        float py = (H / 2.0f - y);
                        Vec3 testDir = (camRight * px + camUp * py + camDir * pz).normalize();
                        Ray  testRay(scene.camera.origin, testDir);
                        if (!traceRay(testRay).did_hit) {
                            // Pure sky pixel — one sample, never revisit
                            Color sky = tracePath(testRay, 0, 8);
                            samplesPer[i]  = minSamples; // skip warmup
                            accumulator[i] = sky;
                            accumSq[i]     = Color(sky.r*sky.r, sky.g*sky.g, sky.b*sky.b);
                            semPerPixel[i] = 0.0f;
                            converged[i]   = true;

                            Color display = sky.reinhardTonemap().gammaCorrect().clampOne();
                            int idx = i * 3;
                            pixels[idx+0] = (uint8_t)(display.r * 255);
                            pixels[idx+1] = (uint8_t)(display.g * 255);
                            pixels[idx+2] = (uint8_t)(display.b * 255);
                            continue; // skip the rest of the pixel loop entirely
                        }
                    }

                    // How many samples to take this frame for this pixel
                    int toTake = 1; // always at least one until minSamples
                    if (samplesPer[i] >= minSamples) {
                        if (converged[i]) {
                            toTake = 0;
                        } else {
                            // Spend more samples where SEM is high
                            // SEM of 0.1 -> 10x budget, SEM of 0.01 -> 1x
                            float ratio = semPerPixel[i] / semThreshold;
                            toTake = std::min(budgetPerFrame, (int) std::ceil(ratio));
                        }
                    }

                    for (int s = 0; s < toTake; s++) {
                        float jx = randomFloat() - 0.5f;
                        float jy = randomFloat() - 0.5f;
                        float px = (x + jx - W / 2.0f);
                        float py = (H / 2.0f - (y + jy));

                        Vec3 worldDir = (camRight * px + camUp * py + camDir * pz).normalize();
                        Ray ray(scene.camera.origin, worldDir);

                        Color sample = tracePath(ray, 0, 8);
                        samplesPer[i]++;
                        accumulator[i] += sample;
                        accumSq[i] += Color(sample.r * sample.r,
                                            sample.g * sample.g,
                                            sample.b * sample.b);
                    }

                    // Recompute SEM from current state
                    int   n    = samplesPer[i];
                    if (n >= 2) {
                        Color mean = accumulator[i] * (1.0f / n);
                        Color sq   = accumSq[i]     * (1.0f / n);
                        float var  = ((sq.r - mean.r*mean.r) +
                                      (sq.g - mean.g*mean.g) +
                                      (sq.b - mean.b*mean.b)) / 3.0f;
                        var = std::max(0.0f, var);

                        float meanLum = (mean.r + mean.g + mean.b) / 3.0f;
                        semPerPixel[i] = sqrt(var / n);

                        // Relative SEM: uncertainty as a fraction of the pixel's own brightness.
                        // A dark pixel that hasn't found the light yet will have a huge relativeSEM
                        // the moment any future sample returns non-zero.
                        float relativeSEM = semPerPixel[i] / (meanLum + 0.001f);

                        // Dark pixels need far more samples before we trust they're truly dark.
                        // If meanLum ~ 0, adaptiveMin grows large; if meanLum ~ 1, it stays at minSamples.
                        int adaptiveMin = (int)(minSamples * (1.0f + 0.05f / (meanLum + 0.001f)));
                        adaptiveMin = std::min(adaptiveMin, 512); // cap so it doesn't run forever

                        if (n >= adaptiveMin
                            && semPerPixel[i] < semThreshold   // absolute confidence
                            && relativeSEM    < 0.5f)          // relative confidence
                                converged[i] = true;
                    }
                    // Display
                    if (n > 0) {
                        Color avg = accumulator[i] * (1.0f / n);
                        Color display = avg.reinhardTonemap().gammaCorrect().clampOne();
                        int idx = i * 3;
                        if (keys[SDL_SCANCODE_TAB]) {
                            // Green   = converged
                            // Red     = still sampling (brightness shows relative SEM urgency)
                            // Yellow  = in minSamples warmup period
                            float urgency = std::min(1.0f, semPerPixel[i] / semThreshold);
                            if (n < minSamples) {
                                pixels[idx+0] = 255;
                                pixels[idx+1] = 255;
                                pixels[idx+2] = 0;   // yellow = warming up
                            } else if (converged[i]) {
                                pixels[idx+0] = 0;
                                pixels[idx+1] = 255;
                                pixels[idx+2] = 0;   // green = done
                            } else {
                                pixels[idx+0] = (uint8_t)(urgency * 255);
                                pixels[idx+1] = 0;
                                pixels[idx+2] = 0;   // red, brighter = more urgent
                            }
                        } else {
                            pixels[idx+0] = (uint8_t)(display.r * 255);
                            pixels[idx+1] = (uint8_t)(display.g * 255);
                            pixels[idx+2] = (uint8_t)(display.b * 255);
                        }
                    }
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

    Color tracePath(const Ray &ray, int depth, int maxDepth) {
        auto skyColor = [&](const Vec3 &dir) {
            return Color(0.01f, 0.01f, 0.01f);
            float t = std::max(0.0f, dir.y);
            return Color(1.0f, 1.0f, 1.0f) * (1.0f - t) + Color(0.3f, 0.6f, 1.0f) * t;
        };

        if (depth >= maxDepth) return skyColor(ray.direction);

        HitResult hit = traceRay(ray);
        if (!hit.did_hit) return skyColor(ray.direction);

        // Emissive surface — return emission directly
        if (hit.material.emissionStrength > 0.0f)
            return hit.material.emission * hit.material.emissionStrength;

        Vec3 normal = hit.normal.normalize();
        if (normal * ray.direction > 0) normal = -normal;

        Vec3 v = (-ray.direction).normalize();
        float rough = std::max(0.001f, hit.material.roughness);
        float alpha = rough * rough;
        float alpha2 = alpha * alpha;

        // F0: metals use albedo color, dielectrics use 0.04
        Color F0 = Color(0.04f, 0.04f, 0.04f) * (1.0f - hit.material.metallic)
                   + hit.material.color * hit.material.metallic;

        // Fresnel at normal incidence decides specular probability
        float F0avg = (F0.r + F0.g + F0.b) / 3.0f;
        float NdotV = std::max(0.0f, normal * v);
        float fresnel = F0avg + (1.0f - F0avg) * pow(1.0f - NdotV, 5.0f);
        float specProb = std::max(0.1f, std::min(0.9f, fresnel));

        Vec3 bounceDir;
        float pdf;
        Color brdf;

        if (randomFloat() < specProb) {
            // ── Specular bounce ──────────────────────────────────────────────
            Vec3 h = Vec3::ggxSample(normal, rough);
            bounceDir = (ray.direction - h * 2.0f * (ray.direction * h)).normalize();

            if (normal * bounceDir <= 0.0f) return Color(0, 0, 0);

            float NdotH = std::max(0.0f, normal * h);
            float NdotL = std::max(0.0f, normal * bounceDir);
            float VdotH = std::max(0.0f, v * h);

            float D = ggxNDF(NdotH, alpha2);
            Color F = schlickFresnel(VdotH, F0);
            float G = smithG(NdotV, NdotL, alpha);

            // Cook-Torrance: D*F*G / (4 * NdotV * NdotL)
            float denom = std::max(1e-6f, 4.0f * NdotV * NdotL);
            brdf = F * (D * G / denom);

            // GGX PDF: D * NdotH / (4 * VdotH)
            pdf = (D * NdotH) / std::max(1e-6f, 4.0f * VdotH);
            pdf *= specProb;
        } else {
            // ── Diffuse bounce ───────────────────────────────────────────────
            bounceDir = Vec3::randomCosineHemisphere(normal);

            float NdotL = std::max(0.0f, normal * bounceDir);
            float VdotH = NdotV;
            Color F = schlickFresnel(VdotH, F0);

            // Lambertian BRDF: albedo / PI, scaled by (1-F) and (1-metallic)
            brdf = (Color(1, 1, 1) - F) * hit.material.color
                   * (1.0f / PI)
                   * (1.0f - hit.material.metallic);

            // Cosine hemisphere PDF: NdotL / PI
            pdf = std::max(0.0f, normal * bounceDir) / PI;
            pdf *= (1.0f - specProb);
        }

        float NdotL = std::max(0.0f, normal * bounceDir);
        if (pdf < 1e-6f) return Color(0, 0, 0);

        Color directLight(0, 0, 0);
        for (const auto &obj: scene.objects) {
            if (obj->material.emissionStrength <= 0.0f) continue;

            // Sample a point on the light (for sphere lights)
            auto *sphere = dynamic_cast<Sphere *>(obj);
            if (!sphere) continue;

            // Random point on light sphere surface
            Vec3 lightSampleDir = Vec3::randomCosineHemisphere(
                (sphere->origin - hit.point).normalize());
            Vec3 lightPoint = sphere->origin + lightSampleDir * sphere->radius;

            Vec3 toLight = lightPoint - hit.point;
            float lightDist = toLight.length();
            Vec3 l = toLight / lightDist;

            float NdotL = normal * l;
            if (NdotL <= 0.0f) continue;

            // Shadow ray
            Ray shadowRay(hit.point + normal * 0.001f, l);
            HitResult shadowHit = traceRay(shadowRay);

            // Only lit if nothing blocks the path to the light
            bool blocked = shadowHit.did_hit &&
                           (shadowHit.point - hit.point).length() < lightDist - 0.01f;
            if (blocked) continue;

            float attenuation = 1.0f / (lightDist * lightDist);
            Color emission = obj->material.emission * obj->material.emissionStrength;
            directLight = directLight + hit.material.color * emission
                          * (NdotL * attenuation);
        }

        Ray bounceRay(hit.point + normal * 0.001f, bounceDir);
        Color incoming = tracePath(bounceRay, depth + 1, maxDepth);

        // Rendering equation: brdf * Li * NdotL / pdf
        return directLight + brdf * incoming * (NdotL / pdf);
    }
};

#endif
