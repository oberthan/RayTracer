#ifndef CLASSES_H
#define CLASSES_H
#include <iostream>
#include <cmath>
#include <memory>
#include <random>
#include <vector>


inline float randomFloat() {
    thread_local std::mt19937 rng(std::random_device{}());
    thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng);
}

// ─── Math helpers ─────────────────────────────────────────────────────────────
const float PI = 3.14159265358979323846f;

inline float ggxNDF(float NdotH, float alpha2) {
    float d = (NdotH * NdotH * (alpha2 - 1.0f) + 1.0f);
    return alpha2 / (PI * d * d);
}

inline float smithG_sub(float NdotV, float alpha) {
    float k = alpha / 2.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

inline float smithG(float NdotV, float NdotL, float alpha) {
    return smithG_sub(NdotV, alpha) * smithG_sub(NdotL, alpha);
}

class Vec3 {
public:
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {
    }

    Vec3(float x, float y, float z)
        : x(x), y(y), z(z) {
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
        return sqrt(*this * *this);
    }

    Vec3 normalize() const {
        return *this / length();
    }

    Vec3 cross(const Vec3 &other) const {
        return {y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x};
    }


    // Build orthonormal basis around a normal — used by both samplers
    static void buildBasis(const Vec3 &normal, Vec3 &tangent, Vec3 &binormal) {
        Vec3 up = fabs(normal.x) > 0.9f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
        tangent = normal.cross(up).normalize();
        binormal = normal.cross(tangent);
    }

    // Cosine-weighted hemisphere sample — for diffuse bounces
    static Vec3 randomCosineHemisphere(const Vec3 &normal) {
        float u1 = randomFloat();
        float u2 = randomFloat();
        float r = sqrt(u1);
        float phi = 2.0f * PI * u2;
        float x = r * cos(phi);
        float z = r * sin(phi);
        float y = sqrt(std::max(0.0f, 1.0f - u1));

        Vec3 tangent, binormal;
        buildBasis(normal, tangent, binormal);
        return (tangent * x + normal * y + binormal * z).normalize();
    }

    // GGX importance sample — for specular bounces
    // Returns the HALFWAY vector, not the bounce direction
    static Vec3 ggxSample(const Vec3 &normal, float roughness) {
        float alpha = roughness * roughness;
        float alpha2 = alpha * alpha;

        float xi1 = randomFloat();
        float xi2 = randomFloat();

        // Sample spherical coords of halfway vector
        float theta = atan(alpha * sqrt(xi1 / std::max(1e-6f, 1.0f - xi1)));
        float phi = 2.0f * PI * xi2;

        float sinTheta = sin(theta);
        float cosTheta = cos(theta);
        Vec3 h(sinTheta * cos(phi), cosTheta, sinTheta * sin(phi));

        // Transform to world space using normal as up axis
        Vec3 tangent, binormal;
        buildBasis(normal, tangent, binormal);
        return (tangent * h.x + normal * h.y + binormal * h.z).normalize();
    }
};

class Ray {
public:
    Vec3 origin, direction;

    Ray() : origin(), direction() {
    } // Fixed: was declared but never defined

    Ray(const Vec3 &origin, const Vec3 &direction)
        : origin(origin), direction(direction) {
    }
};

class Camera {
public:
    Vec3 origin;
    Vec3 direction;
    float focal_length = 600.0f;
    int width = 1920;
    int height = 1080;
    float yaw = 0.0f; // horizontal angle (for mouse look)
    float pitch = 0.0f; // vertical angle   (for mouse look)

    Camera() : origin(0, 0, 0), direction(0, 0, 1) {
    }

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

    Color() : r(0), g(0), b(0) {
    }

    Color(float r, float g, float b) : r(r), g(g), b(b) {
    }

    Color operator*(const Color &other) const {
        return {r * other.r, g * other.g, b * other.b};
    }

    Color operator*(const Vec3 &other) const {
        return {r * other.x, g * other.y, b * other.z};
    }

    Color operator*(float scalar) const {
        return {r * scalar, g * scalar, b * scalar};
    }

    Color operator+(const Color &other) const {
        return {r + other.r, g + other.g, b + other.b};
    }

    Color operator-(const Color &o) const { return {r - o.r, g - o.g, b - o.b}; }

    Color &operator+=(const Color &o) {
        r += o.r;
        g += o.g;
        b += o.b;
        return *this;
    }

    Color clampOne() {
        return {std::min(1.0f, r), std::min(1.0f, g), std::min(1.0f, b)};
    }


    Color reinhardTonemap() const {
        return {r / (r + 1.0f), g / (g + 1.0f), b / (b + 1.0f)};
    }

    Color gammaCorrect() const {
        float inv = 1.0f / 2.2f;
        return {pow(std::max(0.0f, r), inv), pow(std::max(0.0f, g), inv), pow(std::max(0.0f, b), inv)};
    }
};

inline Color schlickFresnel(float VdotH, const Color &F0) {
    float f = pow(1.0f - std::max(0.0f, VdotH), 5.0f);
    return F0 + (Color(1, 1, 1) - F0) * f;
}

class Light {
public:
    Vec3 origin;
    float intensity;
    Color color;

    Light() : intensity(1.0f), color(1, 1, 1) {
    }

    Light(const Vec3 &origin, float intensity, const Color &color)
        : origin(origin), intensity(intensity), color(color) {
    }

    Light(const Vec3 &origin, float intensity)
        : origin(origin), intensity(intensity), color(1, 1, 1) {
    }
};

class Material {
public:
    Color color;
    Color emission;
    float emissionStrength = 0.0f;
    float roughness = 1.0f; // 0 = mirror, 1 = fully diffuse
    float metallic = 0.0f; // 0 = plastic, 1 = metal

    Material() = default;

    Material(const Color &c, float roughness, float metallic = 0.0f)
        : color(c), emission(0, 0, 0), emissionStrength(0),
          roughness(roughness), metallic(metallic) {
    }

    static Material Emissive(const Color &emitColor, float strength) {
        Material m;
        m.color = emitColor;
        m.emission = emitColor;
        m.emissionStrength = strength;
        m.roughness = 1.0f;
        return m;
    }
};

struct HitResult {
    bool did_hit;
    Vec3 point;
    Vec3 basePoint;
    Vec3 normal;
    Material material;
    float u, v;

    HitResult(const Vec3 &p, const Vec3 &b, const Vec3 &n, const Material &m, float u, float v)
        : did_hit(true), point(p), basePoint(b), normal(n), material(m), u(u), v(v) {
    }

    static HitResult NoHit;

private:
    HitResult()
        : did_hit(false), point(), basePoint(), normal(), material(Color(0, 0, 0), 0, 0), u(0), v(0) {
    }
};

HitResult HitResult::NoHit = {};

class RObj {
public:
    virtual ~RObj() = default;

    Vec3 origin;
    Material material;

    RObj(const Vec3 &origin, const Material &material)
        : origin(origin), material(material) {
    }

    virtual HitResult hit(const Ray &ray) const = 0;
};

class Sphere : public RObj {
public:
    float radius;

    explicit Sphere(float radius, const Vec3 &origin, const Material &material)
        : RObj(origin, material), radius(radius) {
    }

    HitResult hit(const Ray &ray) const override {
        Vec3 O = ray.origin - origin;
        float a = ray.direction * ray.direction;
        float b = O * ray.direction * 2;
        float c = O * O - radius * radius;
        float disc = b * b - 4 * a * c;

        if (disc > 0) {
            float t = (-b - sqrt(disc)) / (2 * a);
            if (t > 0) {
                Vec3 p = ray.origin + ray.direction * t;
                Vec3 n = (p - origin).normalize(); // normal points away from sphere center
                return {p, ray.origin, n, material, 0, 0};
            }
        }
        return HitResult::NoHit;
    }
};


class Sine3DFunction : public RObj {
public:
    float ya, yb, yc;
    float xa, xb, xc;
    float d;

    explicit Sine3DFunction(const float &ya, const float &yb, const float &yc,
                            const float &xa, const float &xb, const float &xc,
                            const float &d,
                            const Vec3 &origin, const Material &material)
        : RObj(origin, material),
          ya(ya), yb(yb), yc(yc),
          xa(xa), xb(xb), xc(xc),
          d(d) {}

    float F(float t, const Vec3& O, const Vec3& D) const
    {
        return O.y + t*D.y
            - xa * sin(xb*(O.x + t*D.x) + xc)
            - ya * sin(yb*(O.z + t*D.z) + yc)
            - d;
    }

    float dF(float t, const Vec3& O, const Vec3& D) const
    {
        return D.y
            - xa * xb * D.x * cos(xb*(O.x + t*D.x) + xc)
            - ya * yb * D.z * cos(yb*(O.z + t*D.z) + yc);
    }

    HitResult hit(const Ray& ray) const override {
        Vec3 O = ray.origin - origin;
        Vec3 D = ray.direction;

        // Step 1: ray march to find sign change bracket
        float tStep = 0.05f;
        float tMax  = 4.0f;
        float tPrev = 0.001f;
        float fPrev = F(tPrev, O, D);
        float t     = tPrev;

        bool found = false;
        for (; t < tMax; t += tStep) {
            float f = F(t, O, D);
            if (fPrev * f < 0.0f) { found = true; break; } // sign change!
            fPrev = f;
            tPrev = t;
        }
        if (!found) return HitResult::NoHit;

        // Step 2: refine with Newton-Raphson in the bracket [tPrev, t]
        float tMid = (tPrev + t) * 0.5f;
        for (int i = 0; i < 16; i++) {
            float f  = F(tMid, O, D);
            float df = dF(tMid, O, D);
            if (fabs(df) < 1e-6f) break;
            tMid -= f / df;
            tMid = std::max(tPrev, std::min(t, tMid)); // clamp to bracket
        }
        if (tMid <= 0.001f) return HitResult::NoHit;

        Vec3 p     = ray.origin + ray.direction * tMid;
        Vec3 local = p - origin;
        Vec3 n;
        n.x = -xa * xb * cos(xb * local.x + xc);
        n.y = 1.0f;
        n.z = -ya * yb * cos(yb * local.z + yc);
        return {p, ray.origin, n.normalize(), material, 0, 0};
    }
};

class Rectangle : public RObj {
public:
    Vec3 aV, bV, nV;

    Rectangle(const Vec3 &p0, const Vec3 &p1, const Vec3 &p2, const Material &material)
        : RObj(p0, material), aV(p1 - p0), bV(p2 - p0), nV((p1 - p0).cross(p2 - p0).normalize()) {
    }

    HitResult hit(const Ray &ray) const override {
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
    std::vector<RObj *> objects;


    Scene() = default;

    Scene(const Scene &) = delete;

    Scene &operator=(const Scene &) = delete;

    Scene(Scene &&) = default;

    Scene &operator=(Scene &&) = default;
};

#endif //CLASSES_H
