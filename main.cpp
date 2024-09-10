#include "classes.h"
#include "renderer.h"
#include <iostream>
#include <vector>

#include "renderer.h"
using namespace std;


Scene initializeScene() {
    Scene scene;
    scene.camera.origin = Vec3(0, 0, 0);
    scene.camera.direction = Vec3(0, 0, 1);
    scene.camera.height = 1080;
    scene.camera.width = 1920;
    scene.camera.focal_length = 1000;

    const auto sphere = new Sphere(0.65f,Vec3(-1.25f,0,4),Material(Color(1,1,0.8f), 100, 0.5f));

    scene.objects.push_back(sphere);
    scene.objects.push_back(new Sphere(1.5f, Vec3(0.75f, -0.5f, 4.5f), Material(Color(0.01f,1.0f,0.5f), 10, 0.75f)));

    scene.objects.push_back(new Rectangle(Vec3(3, -1, 0), Vec3(-3,-1,0), Vec3(3,-1, 10), Material(Color(1,0.75f,1.0f), 10, 0.5f)));

    scene.lights.push_back(Light(Vec3(-2,0.5f,1),2));

    return scene;
}

int main() {
    cout << "Hello, World!" << endl;
    Scene scene = initializeScene();
    auto renderer = Renderer(std::move(scene));
    renderer.render();
    return 0;
}