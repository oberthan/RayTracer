//
// Created by jonat on 08-09-2024.
//

#ifndef RENDERER_H
#define RENDERER_H
#include "BMP.h"
#include "classes.h"

class Renderer {
public:
    const Scene scene;


    void render() {
        BMP bmp = BMP(scene.camera.width, scene.camera.height);
        for (int x = 0; x < scene.camera.width; x++) {
            for (int y = 0; y < scene.camera.height; y++) {
                const Vec3 dir = Vec3(x - scene.camera.width / 2, scene.camera.height / 2 - y,
                                      scene.camera.focal_length).normalize();
                Ray ray = Ray(scene.camera.origin, dir);


                Color lightIntesity = calculateLight(ray, 0, 10);

                bmp.SetPixel(x, y, (lightIntesity).clampOne());
            }
        }

        bmp.write("output.bmp");
    }

    HitResult traceRay(const Ray &ray) {
        HitResult closestHit = HitResult::NoHit;
        for (const auto &obj: scene.objects) {
            auto hr = obj->hit(ray);

            if (!hr.did_hit) continue;;
            if (closestHit.did_hit == false || (hr.point - ray.origin).length() < (closestHit.point - ray.origin).
                length()) {
                closestHit = hr;
            }
        }

        return closestHit;
    }

    Color calculateLight(const Ray &ray, int depth, int maxDepth) {
        if (depth > maxDepth) { return Color(0, 0, 0); }
        auto lightColor = Color(0, 0, 0);

        const HitResult hit_result = traceRay(ray);

        if (!hit_result.did_hit) { return Color(0, 0, 0); }

        float diffuse = hit_result.material.diffuse;
        auto normal = hit_result.normal.normalize();
        for (const auto &light: scene.lights) {
            Vec3 l = (light.origin - hit_result.point).normalize();
            auto rl = Ray(hit_result.point, l);

            if (HitResult rlh = traceRay(rl); !rlh.did_hit) {
                float lLen = (l.length() * l.length());

                Vec3 r = (normal * (2 * (l * normal)) - l).normalize();
                Vec3 v = (hit_result.basePoint - hit_result.point).normalize();

                float d = diffuse * (l * normal) * (light.intensity / lLen);
                float s = (1 - diffuse) * pow(v * r, hit_result.material.shininess) * (light.intensity / lLen);

                if (d <= 0) { continue; }
                lightColor = lightColor + light.color * (d + s);
            }
        }

        Vec3 reflect = ray.direction - normal * 2 * (ray.direction * normal);
        auto outRay = Ray(hit_result.point, reflect.normalize());
        auto colorObj = hit_result.material.color;

        Color color = colorObj * lightColor + colorObj * calculateLight(outRay, depth + 1, maxDepth) * (1 - diffuse);
        return color;
    }
};

#endif //RENDERER_H
