#pragma once
#include "core.h"
#include <math.h>

constexpr f32 Pi = 3.1415926535f;

constexpr bool left_handed = true;

static f32 degree_to_rad(f32 degrees)
{
    return degrees * (Pi / 180.0f);
}

static f32 rad_to_degree(f32 radians)
{
    return radians * (180.0f / Pi);
}

// TODO enfore use of these
struct Radians
{
    f32 value;
};

struct Degrees
{
    f32 value;
};

template <typename T>
static T clamp(T v, T min, T max) {
    if (v < min) return min;
    else if (v > max) return max;
    else return v;
}

template <typename T>
static T lerp(T a, T b, f32 t) {
    return a * t + b * (1.0 - t);
}

struct Vec2
{
    f32 x = 0.0f;
    f32 y = 0.0f;

    Vec2& operator+=(Vec2 other);
};

static Vec2 vec2_add(Vec2 lhs, Vec2 rhs)
{
    return Vec2{ lhs.x + rhs.x, lhs.y + rhs.y };
}

static Vec2 vec2_sub(Vec2 lhs, Vec2 rhs)
{
    return Vec2{ lhs.x - rhs.x, lhs.y - rhs.y };
}

static Vec2 vec2_mul(Vec2 lhs, Vec2 rhs)
{
    return Vec2{ lhs.x * rhs.x, lhs.y * rhs.y };
}

static Vec2 vec2_div(Vec2 lhs, Vec2 rhs)
{
    return Vec2{ lhs.x / rhs.x, lhs.y / rhs.y };
}

static Vec2 vec2_mul(Vec2 v, f32 f)
{
    return Vec2{ v.x * f, v.y * f };
}

Vec2& Vec2::operator+=(Vec2 other)
{
    *this = vec2_add(*this, other);
    return *this;
}

static Vec2 operator*(Vec2 lhs, Vec2 rhs)
{
    return vec2_mul(lhs, rhs);
}

static Vec2 operator+(Vec2 lhs, Vec2 rhs)
{
    return vec2_add(lhs, rhs);
}

static Vec2 operator*(Vec2 v, f32 f)
{
    return vec2_mul(v, f);
}

struct Vec3
{
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;

    Vec3& operator+=(Vec3 other);
    Vec3& operator*=(Vec3 other);
    Vec3& operator-=(Vec3 other);

    Vec3& operator+=(f32 val);
    Vec3& operator*=(f32 val);

    f32& operator[](int i)
    {
        ASSERT(i >= 0 && i <= 2);
        return ((f32*)this)[i];
    }
};

static Vec3 vec3_zero()
{
    return Vec3{ 0.f, 0.f, 0.f };    
}

static Vec3 negate(Vec3 v)
{
    return Vec3{-v.x, -v.y, -v.z};
}

static Vec3 operator-(Vec3 v)
{
    return negate(v);
}

static Vec3 clamp(Vec3 v, f32 min, f32 max) {
    return Vec3 {
        clamp(v.x, min, max),
        clamp(v.y, min, max),
        clamp(v.z, min, max)
    };
}

static Vec3 vec3_mul(Vec3 lhs, Vec3 rhs)
{
    return Vec3
    {
        lhs.x * rhs.x,
        lhs.y * rhs.y,
        lhs.z * rhs.z,
    };
}

static Vec3 vec3_add(Vec3 lhs, Vec3 rhs)
{
    return Vec3
    {
        lhs.x + rhs.x,
        lhs.y + rhs.y,
        lhs.z + rhs.z,
    };
}

static Vec3 vec3_sub(Vec3 lhs, Vec3 rhs)
{
    return Vec3
    {
        lhs.x - rhs.x,
        lhs.y - rhs.y,
        lhs.z - rhs.z,
    };
}

static Vec3 vec3_mul(Vec3 lhs, f32 rhs)
{
    return Vec3
    {
        lhs.x * rhs,
        lhs.y * rhs,
        lhs.z * rhs,
    };
}

static Vec3 vec3_add(Vec3 lhs, f32 rhs)
{
    return Vec3
    {
        lhs.x + rhs,
        lhs.y + rhs,
        lhs.z + rhs,
    };
}

static Vec3 vec3_div(Vec3 lhs, f32 rhs)
{
    return Vec3
    {
        lhs.x / rhs,
        lhs.y / rhs,
        lhs.z / rhs,
    };
}

Vec3& Vec3::operator+=(Vec3 other)
{
    *this = vec3_add(*this, other);
    return *this;
}

Vec3& Vec3::operator*=(Vec3 other)
{
    *this = vec3_mul(*this, other);
    return *this;
}

Vec3& Vec3::operator-=(Vec3 other)
{
    *this = vec3_sub(*this, other);
    return *this;
}

Vec3& Vec3::operator+=(f32 val)
{
    *this = vec3_add(*this, val);
    return *this;
}

Vec3& Vec3::operator*=(f32 val)
{
    *this = vec3_mul(*this, val);
    return *this;
}

static Vec3 operator+(Vec3 lhs, Vec3 rhs)
{
    return vec3_add(lhs, rhs);
}

static Vec3 operator*(Vec3 lhs, Vec3 rhs)
{
    return vec3_mul(lhs, rhs);
}

static Vec3 operator-(Vec3 lhs, Vec3 rhs)
{
    return vec3_sub(lhs, rhs);
}

static Vec3 operator+(Vec3 v, f32 s)
{
    return vec3_add(v, s);
}

static Vec3 operator*(Vec3 v, f32 s)
{
    return vec3_mul(v, s);
}

static f32 magnitude(Vec3 v)
{
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static Vec3 normalized(Vec3 v)
{
    return vec3_div(v, magnitude(v));
}

static f32 dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 cross(Vec3 a, Vec3 b)
{
    return Vec3 {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

struct Vec4
{
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 0.0f;

    f32& operator[](int i)
    {
        ASSERT(i >= 0 && i <= 3);
        return ((f32*)this)[i];
    }
};

// We use column major convention.
// https://fgiesen.wordpress.com/2012/02/12/row-major-vs-column-major-row-vectors-vs-column-vectors/

struct Matrix4
{
    union
    {
        f32 m[16];
        struct
        {
            f32 m00; f32 m10; f32 m20; f32 m30;
            f32 m01; f32 m11; f32 m21; f32 m31;
            f32 m02; f32 m12; f32 m22; f32 m32;
            f32 m03; f32 m13; f32 m23; f32 m33;
        };
    };

    Matrix4() = default;

    Matrix4(f32 m00, f32 m01, f32 m02, f32 m03,
            f32 m10, f32 m11, f32 m12, f32 m13,
            f32 m20, f32 m21, f32 m22, f32 m23,
            f32 m30, f32 m31, f32 m32, f32 m33)
    {
        this->m00 = m00;
        this->m10 = m10;
        this->m20 = m20;
        this->m30 = m30;

        this->m01 = m01;
        this->m11 = m11;
        this->m21 = m21;
        this->m31 = m31;

        this->m02 = m02;
        this->m12 = m12;
        this->m22 = m22;
        this->m32 = m32;

        this->m03 = m03;
        this->m13 = m13;
        this->m23 = m23;
        this->m33 = m33;
    }

    f32& operator()(u32 row, u32 col)
    {
        return m[4 * col + row];
    }

    f32 const& operator()(u32 row, u32 col) const
    {
        return m[4 * col + row];
    }
};

static Matrix4 mat4_zero()
{
    return Matrix4(
        0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f
    );
}

static Matrix4 mat4_identity()
{
    return Matrix4(
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    );
}

static bool mat4_eq(Matrix4 const& lhs, Matrix4 const& rhs)
{
    for (u32 row = 0; row < 4; ++row)
    {
        for (u32 col = 0; col < 4; ++col)
        {
            if (lhs(row, col) != rhs(row, col)) return false;
        }
    }
    return true;
}

static Matrix4 mat4_mul(Matrix4 const& lhs, Matrix4 const& rhs)
{
    Matrix4 result = mat4_zero();

    for (u32 row = 0; row < 4; ++row)
    {
        for (u32 col = 0; col < 4; ++col)
        {
            f32& field = result(row, col);
            field += lhs(row, 0) * rhs(0, col);
            field += lhs(row, 1) * rhs(1, col);
            field += lhs(row, 2) * rhs(2, col);
            field += lhs(row, 3) * rhs(3, col);
        }
    }

    return result;
}

static Vec4 mat4_mul(Matrix4 const& m, Vec4 v)
{
    Vec4 out;
    for (int i = 0; i < 4; ++i)
    {
        out[i] = m(i, 0) * v[0] + m(i, 1) * v[1] + m(i, 2) * v[2] + m(i, 3) * v[3]; 
    }
    return out;
}

static void test_mat4_mul()
{
    Matrix4 lhs(
        1.f, 8.f, 4.f, 5.f,
        6.f, 2.f, 1.f, 7.f,
        3.f, 9.f, 9.f, 2.f,
        8.f, 6.f, 4.f, 5.f
    );

    Matrix4 rhs(
        8.f, 2.f, 9.f, 2.f,
        3.f, 5.f, 4.f, 1.f,
        7.f, 6.f, 3.f, 2.f,
        9.f, 8.f, 5.f, 7.f
    );

    Matrix4 l_to_r = mat4_mul(lhs, rhs);
    Matrix4 l_to_r_expected(
        105.f, 106.f, 78.f, 53.f,
        124.f, 84.f, 100.f, 65.f,
        132.f, 121.f, 100.f, 47.f,
        155.f, 110.f, 133.f, 65.f
    );

    ASSERT(mat4_eq(l_to_r_expected, l_to_r));

    Matrix4 r_to_l = mat4_mul(rhs, lhs);
    Matrix4 r_to_l_expected(
        63.f, 161.f, 123.f, 82.f,
        53.f, 76.f, 57.f, 63.f,
        68.f, 107.f, 69.f, 93.f,
        128.f, 175.f, 117.f, 146.f
    );

    ASSERT(mat4_eq(r_to_l_expected, r_to_l));
}

static Matrix4 mat4_translate(Vec3 translation)
{
    Matrix4 result = mat4_identity();
    result.m03 = translation.x;
    result.m13 = translation.y;
    result.m23 = translation.z;
    return result;
}

static Matrix4 mat4_translate(f32 x, f32 y, f32 z)
{
    return mat4_translate(Vec3{x, y, z});
}

namespace detail
{
    static Matrix4 mat4_rotate_LH(Vec3 angles_rad)
    {
        f32 const A = cosf(angles_rad.x);
        f32 const B = sinf(angles_rad.x);
        f32 const C = cosf(angles_rad.y);
        f32 const D = sinf(angles_rad.y);
        f32 const E = cosf(angles_rad.z);
        f32 const F = sinf(angles_rad.z);

        return Matrix4(
            C * E,             -C * F,             -D,     0.0f,
            -B * D * E + A * F, B * D * F + A * E, -B * C, 0.0f,
            A * D * E + B * F, -A * D * F + B * E, A * C,  0.0f,
            0.0f,              0.0f,               0.0f,   1.0f
        );
    }

    static Matrix4 mat4_rotate_RH(Vec3 angles_rad)
    {
        Vec3 flipped_angles = angles_rad;
        flipped_angles.y *= -1;
        return mat4_rotate_LH(flipped_angles);
    }
}

static Matrix4 mat4_rotate(Vec3 angles_rad)
{
    if constexpr (left_handed)
    {
        return detail::mat4_rotate_LH(angles_rad);
    }
    else
    {
        return detail::mat4_rotate_RH(angles_rad);
    }
}

static Matrix4 mat4_rotate(f32 rad_x, f32 rad_y, f32 rad_z)
{
    return mat4_rotate(Vec3{rad_x, rad_y, rad_z});
}

namespace detail
{
    // note for future: http://perry.cz/articles/ProjectionMatrix.xhtml
    static Matrix4 mat4_perspective_LH(f32 vertical_fov_rad, f32 aspect_ratio, f32 near_z, f32 far_z)
    {
        f32 g = 1.0f / tanf(vertical_fov_rad * 0.5f);

        return Matrix4(
            g / aspect_ratio, 0, 0, 0,
            0, g, 0, 0,
            0, 0, far_z / (far_z - near_z), -near_z * (far_z / (far_z - near_z)),
            0, 0, 1.f, 0.f);
    }

    static Matrix4 mat4_perspective_RH(f32 vertical_fov_rad, f32 aspect_ratio, f32 near_z, f32 far_z)
    {
        f32 g = 1.0f / tanf(vertical_fov_rad * 0.5f);
        f32 k = far_z / (far_z - near_z);

        return Matrix4(
            g / aspect_ratio, 0, 0, 0,
            0, -g, 0, 0,
            0, 0, k, -near_z * k,
            0, 0, 1.f, 0.f);
    }
}

static Matrix4 mat4_perspective(f32 vertical_fov_rad, f32 aspect_ratio, f32 near_z, f32 far_z)
{
    if constexpr (left_handed)
    {
        return detail::mat4_perspective_LH(vertical_fov_rad, aspect_ratio, near_z, far_z);
    }
    else
    {
        return detail::mat4_perspective_RH(vertical_fov_rad, aspect_ratio, near_z, far_z);
    }
}

static Matrix4 transpose(Matrix4 const& m)
{
    Matrix4 out = m;

    for (int i = 0; i < 4; i++)
    {
        for (int j = i + 1; j < 4; j++)
        {
            f32 temp = out(i, j);
            out(i, j) = out(j, i);
            out(j, i) = temp;
        }
    }
    return out;
}

namespace detail
{
    static Matrix4 mat4_look_to_lh(Vec3 eye_pos, Vec3 eye_dir, Vec3 up)
    {
        Vec3 R2 = normalized(eye_dir);
        Vec3 R0 = normalized(cross(up, R2));
        Vec3 R1 = cross(R2, R0);

        Vec3 NegEyePosition = negate(eye_pos);
        f32 D0 = dot(R0, NegEyePosition);
        f32 D1 = dot(R1, NegEyePosition);
        f32 D2 = dot(R2, NegEyePosition);

        Matrix4 m(
            R0.x, R0.y, R0.z, D0,
            R1.x, R1.y, R1.z, D1,
            R2.x, R2.y, R2.z, D2,
            0.f, 0.f, 0.f, 1.f);

        return m;
    }

    static Matrix4 mat4_look_at_lh(Vec3 cameraPos, Vec3 target, Vec3 up)
    {
        Vec3 eye_dir = vec3_sub(target, cameraPos);
        return mat4_look_to_lh(cameraPos, eye_dir, up);
    }

    static Matrix4 mat4_look_at_rh(Vec3 cameraPos, Vec3 target, Vec3 up)
    {
        Vec3 neg_eye_dir = vec3_sub(cameraPos, target);
        return mat4_look_to_lh(cameraPos, neg_eye_dir, up);
    }
}

static Matrix4 mat4_look_at(Vec3 cameraPos, Vec3 target, Vec3 up)
{
    if constexpr (left_handed)
    {
        return detail::mat4_look_at_lh(cameraPos, target, up);
    }
    else
    {
        return detail::mat4_look_at_rh(cameraPos, target, up);
    }
}