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
        for (int x= 0;x<scene.camera.width;x++) {
            for (int y= 0;y<scene.camera.height;y++) {
                const Vec3 dir = Vec3(x-scene.camera.width/2, scene.camera.height/2-y, scene.camera.focal_length).normalize();
                Ray ray = Ray(scene.camera.origin, dir);



                Color lightIntesity = calculateLight(ray, 0, 20);
                //std::cout << (lightIntesity*result.material.color).r << std::endl;

                bmp.SetPixel(x,y,(lightIntesity).clampOne());
            }
        }

        bmp.write("output.bmp");
    }

    HitResult traceRay(const Ray& ray) {
        HitResult closestHit = HitResult::NoHit;
        for (const auto& obj: scene.objects) {
            auto hr = obj->hit(ray);

            if (!hr.did_hit) continue;;
            if (closestHit.did_hit==false||(hr.point-ray.origin).length() < (closestHit.point-ray.origin).length()) {
                closestHit = hr;
            }
            //std::cout << closestHit.did_hit <<hr.did_hit << std::endl;
        }
        //std::cout << closestHit.did_hit << std::endl;



        return closestHit;

        /*for (auto it=scene.objects.begin(); it!=scene.objects.end(); ++it) {
         auto hr = it->hit(ray);
        }


        for (int i=0; i < scene.objects.size(); i++) {
            HitResult hr = scene.objects[i].hit(ray);

        }*/
    }
    Color calculateLight(const Ray& ray, int depth, int maxDepth) {

        if (depth > maxDepth) {return Color(0, 0, 0);}
        float lightColor = 0;

        HitResult hit_result = traceRay(ray);

        if (!hit_result.did_hit) {return Color(0, 0, 0);}

        for (const auto& light: scene.lights) {
            //std::cout << light.origin.x << std::endl;
            Vec3 l = (light.origin - hit_result.point).normalize();
            Ray rl = Ray(hit_result.point, l);
            HitResult rlh = traceRay(rl);

            if (!rlh.did_hit) {
                //std::cout << rlh.did_hit << std::endl;

                float d = hit_result.material.diffuse*(l*hit_result.normal.normalize())*( light.intensity/(l.length()*l.length()));
                Vec3 v = (hit_result.basePoint-hit_result.point).normalize();
                Vec3 r = (hit_result.normal.normalize()*(2*(l*hit_result.normal.normalize()))-l).normalize();
                float s = (1-hit_result.material.diffuse)*pow(v*r, hit_result.material.shininess)*(light.intensity/(l.length()*l.length()));
                if (d <= 0) {continue;}
                lightColor+=d+s;

            }
        }

        Vec3 reflect = ray.direction - hit_result.normal.normalize()*2 * (ray.direction*hit_result.normal.normalize());
        Ray outRay = Ray(hit_result.point, reflect.normalize());
        //std::cout << lightColor << std::endl;
        Color color = hit_result.material.color*lightColor + calculateLight(outRay, depth+1, maxDepth)*0.3f;
        return color;

    }
};

#endif //RENDERER_H
