//
// Created by jonat on 08-09-2024.
//

#ifndef CLASSES_H
#define CLASSES_H
#include <math.h>
#include <valarray>
#include <vector>

class Vec3 {
public:
    float x, y, z;

    Vec3(float x, float y, float z)
        : x(x),
          y(y),
          z(z) {
    }
    Vec3 operator+(const Vec3 &other) const {
        return {x + other.x, y + other.y, z + other.z};
    }

    Vec3 operator-() const {
        return {-x, -y, -z};
    }

    Vec3 operator-(const Vec3 &other) const {
        return {x - other.x, y - other.y, z - other.z};
    }

    Vec3 operator*(float scalar) const {
        return {x * scalar, y * scalar, z * scalar};
    }

    float operator*(const Vec3 &other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    Vec3 operator/(float scalar) const {
        return {x / scalar, y / scalar, z / scalar};
    }

    float length() const {
        return sqrt(*this**this);
    }
    Vec3 normalize() const {
        return *this / length();
    }
};
class Ray {
public:
    Vec3 origin, direction;

    Ray();

    Ray(const Vec3 &origin, const Vec3 &direction)
        : origin(origin),
          direction(direction) {
    }
};

class Camera {
public:
    Vec3 origin, direction;
    float focal_length;
    int width, height;

    Camera();
};

class Light {
public:
    Vec3 origin;
    float intensity;
};

class Hit_record {
};

class Color {
public:
    float r, g, b;
};
struct HitResult {
    bool did_hit;
    Vec3 point;
    Vec3 normal;
    Color color;

    HitResult(const Vec3& p, const Vec3& n, const Color& c) : did_hit(true), point(p), normal(n), color(c) {}

private:
    HitResult() : did_hit(false), point(0,0,0), normal(0,0,0), color(0,0,0) {}

public:
    static HitResult NoHit;
};

class RObj {
public:
    Vec3 origin;

    virtual HitResult hit(const Ray& ray) = 0;
};

class sphere:RObj {
public:
    float radius;
    Color color;

    HitResult hit(const Ray& ray) override {
        Vec3 oc = origin - ray.origin;
        if (oc*ray.direction <0) return HitResult::NoHit;
        else {
            float a = ray.direction*ray.direction;
            float b = ray.origin*ray.direction*2;
            float c = ray.origin*ray.origin - radius*radius;
            if (b*b-4*a*c > 0) {
                auto p = ray.direction*(-b-sqrt(b*b-4*a*c))/(2*a);
                auto n = p-origin;
                return {p,n,color};
            }
        }
        return HitResult::NoHit;


    }
};


class Scene {
public:
    Camera camera;
    std::vector<RObj> objects;
    std::vector<Light> lights;

    Scene();
};

#endif //CLASSES_H
