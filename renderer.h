//
// Created by jonat on 08-09-2024.
//

#ifndef RENDERER_H
#define RENDERER_H
#include "classes.h"

class Renderer {
    public:
    const Scene scene;


    int render() {
        for (int x= -scene.camera.width/2;x<scene.camera.width/2;x++) {
            for (int y= -scene.camera.height/2;y<scene.camera.height/2;y++) {
                const Vec3 dir = Vec3(x, y, scene.camera.focal_length).normalize();
                Ray ray = Ray(scene.camera.origin, dir);

                traceRay(ray);
            }
        }
    }

    Color traceRay(const Ray& ray) {
        for (int i=0; i < scene.objects.size(); i++) {


        }
    }
};

#endif //RENDERER_H
