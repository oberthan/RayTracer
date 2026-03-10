#include "classes.h"
#include "renderer.h"
#include <iostream>
#include <vector>

#include "renderer.h"
using namespace std;


Scene initializeScene() {
    Scene scene;
    scene.camera.origin = Vec3(0, 1, 0);
    scene.camera.direction = Vec3(0, 0, 1);
    scene.camera.width = 220;
    scene.camera.height = scene.camera.width*9/16;
    scene.camera.focal_length = 75;

    // Diffuse blue sphere
    scene.objects.push_back(new Sphere(0.65f, Vec3(-1.25f, 0.65f, 4),
        Material(Color(0.1f, 0.2f, 1.0f), 1.0f, 0.0f)));

    // Rough metal sphere
    scene.objects.push_back(new Sphere(1.5f, Vec3(1.75f, 1.5f, 4.5f),
        Material(Color(0.0f, 0.8f, 0.0f), 0.2f, 1.0f)));

    // Floor


    scene.objects.push_back(
    new Sine3DFunction(
        0.25f, 6.0f, 0.0f,
        0.25f, 6.0f, 0.0f,
        0.0f,
        Vec3(0,0.0f,0),
        Material(Color(0.1f,0.2f,1.0f),0.5f,0.2f)
    )
);

    // Emissive light sphere
    scene.objects.push_back(new Sphere(0.5f, Vec3(0, 4.0f, 3),
        Material::Emissive(Color(1.0f, 0.95f, 0.8f), 15.0f)));

    // Emissive light sphere
    scene.objects.push_back(new Sphere(0.5f, Vec3(16, 4.0f, 3),
        Material::Emissive(Color(1.0f, 0.95f, 0.8f), 15.0f)));

    return scene;
}

int main() {
    Scene scene = initializeScene();
    auto renderer = Renderer(std::move(scene));
    renderer.render();
    return 0;
}