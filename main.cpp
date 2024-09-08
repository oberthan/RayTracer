#include "classes.h"
#include <iostream>
#include <vector>
using namespace std;

int main() {
    cout << "Hello, World!" << endl;

    return 0;
}
Scene initializeScene() {
    Scene scene;
    scene.camera.origin = Vec3(0, 0, 0);
    scene.camera.direction = Vec3(1, 0, 0);

}

