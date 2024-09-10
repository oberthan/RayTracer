//
// Created by jonat on 08-09-2024.
//

#ifndef CLASSES_H
#define CLASSES_H
#include <iostream>
#include <cmath>
#include <memory>
#include <valarray>
#include <vector>

class Vec3 {
public:
    float x, y, z;

    Vec3() = default;

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
    Vec3 cross(const Vec3 &other) const {
        return {y*other.z-z*other.y, z*other.x-x*other.z, x*other.y-y*other.x};
    };


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

    Camera() = default;
};

class Light {
public:
    Vec3 origin;
    float intensity;

    Light() = default;

    Light(const Vec3 &origin, float intensity)
        : origin(origin),
          intensity(intensity) {
    }
};

class Hit_record {
};

class Color {
public:
    float r, g, b;

    Color operator*(const Color &other) const {
        return {r*other.r, g*other.g, b*other.b};
    }
    Color operator*(const Vec3 &other) const {
        return {r*other.x, g*other.y, b*other.z};
    }
    Color operator*(float scalar) const {
        return {r*scalar, g*scalar, b*scalar};
    }

    Color operator+(const Color &other) const {
        return {r+other.r, g+other.g, b+other.b};
    }

    Color clampOne() {
        return {std::min(1.0f,r), std::min(1.0f,g), std::min(1.0f,b)};
    }
};
class Material {
public:
    Color color;
    float shininess;
    float diffuse;

    Material() = default;
    Material(const Color& c, float s, float d) : color(c), shininess(s), diffuse(d) {}
};

struct HitResult {
    bool did_hit;
    Vec3 point;
    Vec3 basePoint;
    Vec3 normal;
    Material material;
    float u,v;

    HitResult(const Vec3& p,const Vec3& b, const Vec3& n, const Material& m, const float u, const float v) : did_hit(true), point(p), basePoint(b), normal(n), material(m), u(u), v(v) {}

private:
    HitResult() : did_hit(false), point(0,0,0), basePoint(0,0,0), normal(0,0,0), material(Color(0,0,0),0,0), u(0), v(0) {}

public:
    static HitResult NoHit;
};
HitResult HitResult::NoHit = { };


class RObj {
public:
    virtual ~RObj() = default;

    Vec3 origin;
    Material material;

    RObj(const Vec3 &origin, const Material &material)
        : origin(origin),
          material(material) {
    }

    virtual HitResult hit(const Ray& ray) const = 0;
};

class Sphere : public RObj {
public:
    float radius;


    HitResult hit(const Ray& ray) const override {
        Vec3 oc = origin - ray.origin;
        if (oc*ray.direction <0) return HitResult::NoHit;
        else {
            Vec3 O = ray.origin - origin;
            float a = ray.direction*ray.direction;
            float b = O*ray.direction*2;
            float c = O*O - radius*radius;

            if (b*b-4*a*c > 0) {
                float t =(-b-sqrt(b*b-4*a*c))/(2*a);
                if (t > 0) {
                    auto p = ray.direction*t;
                    auto n = p-origin;

                    float u = 0;
                    float v = 0;

                    return {p,ray.origin,n,material, u, v};
                }
            }
        }
        return HitResult::NoHit;
    }

    explicit Sphere(const float radius, const Vec3 &origin, const Material &material)
        : RObj(origin, material), radius(radius) {
    }
};

class Rectangle : public RObj {
    public:
    Vec3 aV;
    Vec3 bV;
    Vec3 nV;

    HitResult hit(const Ray& ray) const override {
        // Tomas Möller and Ben Trumbore "Fast, Minimum Storage Ray/Triangle Intersection" - 1997
        Vec3 pV = ray.direction.cross(bV);
        float det = aV * pV;
        // if the determinant is negative the rectangle is back facing
        // if the determinant is close to 0, the ray misses the rectangle
        // Check for abs determinant for double sided rectangles
        if (det <= 0) return HitResult::NoHit;

        float invDet = 1.0 / det;

        Vec3 tV = ray.origin - origin;
        float u = (tV * pV) * invDet;
        if (u < 0 || u > 1) return HitResult::NoHit;

        Vec3 qV = tV.cross(aV);
        float v = (ray.direction * qV) * invDet;

        // The intersection check is identical to the triangle intersection check except for u+v > 1 replaced by v > 1
        if (v < 0 || v > 1) return HitResult::NoHit;

        float t = (bV* qV) * invDet;

        Vec3 intersectionPoint = ray.origin + ray.direction * t;
        return {intersectionPoint, ray.origin, nV, material, u, v};
    }

    Rectangle(const Vec3 &p0, const Vec3 &p1, const Vec3 &p2, const Material &material)
        : RObj(p0, material), aV(p1-p0), bV(p2-p0), nV((p1-p0).cross(p2-p0).normalize()) {
    }
};

class Scene {
public:
    Camera camera{};
    std::vector<RObj*> objects;
    std::vector<Light> lights;

    Scene() = default;

    // Delete copy constructor and copy assignment operator (because of unique_ptr)
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Enable move constructor and move assignment operator
    Scene(Scene&&) = default;
    Scene& operator=(Scene&&) = default;

};

#endif //CLASSES_H
