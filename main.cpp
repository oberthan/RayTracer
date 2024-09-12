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
    scene.camera.focal_length = 600;

    const auto sphere = new Sphere(0.65f,Vec3(-2.25f,1,4),Material(Color(0.1f,0.2f,1.0f), 100, 0.2f));

    scene.objects.push_back(sphere);
    scene.objects.push_back(new Sphere(1.5f, Vec3(1.75f, 1.5f, 4.5f), Material(Color(0.09f,1.0f,0.3f), 10, 0.2f)));

    scene.objects.push_back(new Rectangle(Vec3(3, -0.01, 0), Vec3(-3,-0.0f,0), Vec3(3,-0.0f, 10), Material(Color(0.99f,1,1.0f), 10, 0.1f)));


    scene.lights.push_back(Light(Vec3(-2,3.5f,1),6, Color(1,1,1)));

    return scene;
}

int main() {
    cout << "Hello, World!" << endl;
    Scene scene = initializeScene();
    auto renderer = Renderer(std::move(scene));
    renderer.render();
    return 0;
}