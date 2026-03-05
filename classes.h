#ifndef CLASSES_H
#define CLASSES_H
#include <iostream>
#include <cmath>
#include <memory>
#include <vector>

class Vec3 {
public:
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}

    Vec3(float x, float y, float z)
        : x(x), y(y), z(z) {}

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
        return sqrt(*this * *this);
    }
    Vec3 normalize() const {
        return *this / length();
    }
    Vec3 cross(const Vec3 &other) const {
        return {y*other.z - z*other.y, z*other.x - x*other.z, x*other.y - y*other.x};
    }
};

class Ray {
public:
    Vec3 origin, direction;

    Ray() : origin(), direction() {}  // Fixed: was declared but never defined

    Ray(const Vec3 &origin, const Vec3 &direction)
        : origin(origin), direction(direction) {}
};

class Camera {
public:
    Vec3 origin;
    Vec3 direction;
    float focal_length = 600.0f;
    int width = 1920;
    int height = 1080;
    float yaw   = 0.0f;   // horizontal angle (for mouse look)
    float pitch = 0.0f;   // vertical angle   (for mouse look)

    Camera() : origin(0,0,0), direction(0,0,1) {}

    // Recompute direction from yaw and pitch
    void updateDirection() {
        direction = Vec3(
            cos(pitch) * sin(yaw),
            sin(pitch),
            cos(pitch) * cos(yaw)
        ).normalize();
    }
};

class Color {
public:
    float r, g, b;

    Color() : r(0), g(0), b(0) {}
    Color(float r, float g, float b) : r(r), g(g), b(b) {}

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

class Light {
public:
    Vec3 origin;
    float intensity;
    Color color;

    Light() : intensity(1.0f), color(1,1,1) {}

    Light(const Vec3 &origin, float intensity, const Color &color)
        : origin(origin), intensity(intensity), color(color) {}

    Light(const Vec3 &origin, float intensity)
        : origin(origin), intensity(intensity), color(1,1,1) {}
};

class Material {
public:
    Color color;
    float shininess = 0.0f;
    float diffuse   = 1.0f;

    Material() = default;
    Material(const Color& c, float s, float d) : color(c), shininess(s), diffuse(d) {}
};

struct HitResult {
    bool did_hit;
    Vec3 point;
    Vec3 basePoint;
    Vec3 normal;
    Material material;
    float u, v;

    HitResult(const Vec3& p, const Vec3& b, const Vec3& n, const Material& m, float u, float v)
        : did_hit(true), point(p), basePoint(b), normal(n), material(m), u(u), v(v) {}

    static HitResult NoHit;

private:
    HitResult()
        : did_hit(false), point(), basePoint(), normal(), material(Color(0,0,0),0,0), u(0), v(0) {}
};
HitResult HitResult::NoHit = {};

class RObj {
public:
    virtual ~RObj() = default;

    Vec3 origin;
    Material material;

    RObj(const Vec3 &origin, const Material &material)
        : origin(origin), material(material) {}

    virtual HitResult hit(const Ray& ray) const = 0;
};

class Sphere : public RObj {
public:
    float radius;

    explicit Sphere(float radius, const Vec3 &origin, const Material &material)
        : RObj(origin, material), radius(radius) {}

    HitResult hit(const Ray& ray) const override {
        Vec3 O = ray.origin - origin;
        float a = ray.direction * ray.direction;
        float b = O * ray.direction * 2;
        float c = O * O - radius * radius;
        float disc = b*b - 4*a*c;

        if (disc > 0) {
            float t = (-b - sqrt(disc)) / (2*a);
            if (t > 0) {
                Vec3 p = ray.origin + ray.direction * t;
                Vec3 n = (p - origin).normalize();  // normal points away from sphere center
                return {p, ray.origin, n, material, 0, 0};
            }
        }
        return HitResult::NoHit;
    }
};

class Rectangle : public RObj {
public:
    Vec3 aV, bV, nV;

    Rectangle(const Vec3 &p0, const Vec3 &p1, const Vec3 &p2, const Material &material)
        : RObj(p0, material), aV(p1-p0), bV(p2-p0), nV((p1-p0).cross(p2-p0).normalize()) {}

    HitResult hit(const Ray& ray) const override {
        Vec3 pV = ray.direction.cross(bV);
        float det = aV * pV;
        if (det <= 0) return HitResult::NoHit;

        float invDet = 1.0f / det;
        Vec3 tV = ray.origin - origin;
        float u = (tV * pV) * invDet;
        if (u < 0 || u > 1) return HitResult::NoHit;

        Vec3 qV = tV.cross(aV);
        float v = (ray.direction * qV) * invDet;
        if (v < 0 || v > 1) return HitResult::NoHit;

        float t = (bV * qV) * invDet;
        Vec3 intersectionPoint = ray.origin + ray.direction * t;
        return {intersectionPoint, ray.origin, nV, material, u, v};
    }
};

class Scene {
public:
    Camera camera{};
    std::vector<RObj*> objects;
    std::vector<Light> lights;

    Scene() = default;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    Scene(Scene&&) = default;
    Scene& operator=(Scene&&) = default;
};

#endif //CLASSES_H