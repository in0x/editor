#pragma once
#include "core.h"
#include <math.h>

constexpr f32 Pi = 3.1415926535f;

static f32 degree_to_rad(f32 degrees)
{
    return degrees * (Pi / 180.0f);
}

static f32 rad_to_degree(f32 radians)
{
    return radians * (180.0f / Pi);
}

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

struct Vector3
{
    f32 x = 0.0f;        
    f32 y = 0.0f;        
    f32 z = 0.0f;        

    Vector3& operator+=(Vector3 const& other);
    Vector3& operator*=(Vector3 const& other);

    Vector3& operator+=(f32 val);
    Vector3& operator*=(f32 val);
};

static Vector3 clamp(Vector3 v, f32 min, f32 max) {
    return Vector3 {
        clamp(v.x, min, max),
        clamp(v.y, min, max),
        clamp(v.z, min, max)
    };
}

Vector3 vec3_mul(Vector3 const& lhs, Vector3 const& rhs)
{
    return Vector3
    {
        lhs.x * rhs.x,
        lhs.y * rhs.y,
        lhs.z * rhs.z,
    };
}

Vector3 vec3_add(Vector3 const& lhs, Vector3 const& rhs)
{
    return Vector3 
    {
        lhs.x + rhs.x,
        lhs.y + rhs.y,
        lhs.z + rhs.z,
    };
}

Vector3 vec3_mul(Vector3 const& lhs, f32 rhs)
{
    return Vector3
    {
        lhs.x * rhs,
        lhs.y * rhs,
        lhs.z * rhs,
    };
}

Vector3 vec3_add(Vector3 const& lhs, f32 rhs)
{
    return Vector3 
    {
        lhs.x + rhs,
        lhs.y + rhs,
        lhs.z + rhs,
    };
}

Vector3& Vector3::operator+=(Vector3 const& other)
{
    *this = vec3_add(*this, other);
    return *this;
}

Vector3& Vector3::operator*=(Vector3 const& other)
{
    *this = vec3_mul(*this, other);
    return *this;
}

Vector3& Vector3::operator+=(f32 val)
{
    *this = vec3_add(*this, val);
    return *this;
}

Vector3& Vector3::operator*=(f32 val)
{
    *this = vec3_mul(*this, val);
    return *this;
}

static Vector3 operator+(Vector3 const& lhs, Vector3 const& rhs)
{
    return vec3_add(lhs, rhs);
}

static Vector3 operator*(Vector3 const& lhs, Vector3 const& rhs)
{
    return vec3_mul(lhs, rhs);
}

static Vector3 operator+(Vector3 const& v, f32 s)
{
    return vec3_add(v, s);
}

static Vector3 operator*(Vector3 const& v, f32 s)
{
    return vec3_mul(v, s);
}

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

static Matrix4 matrix4_zero()
{
    return Matrix4(
        0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f
    );
}

static Matrix4 matrix4_identity()
{
    return Matrix4(
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    );
}

static bool matrix4_eq(Matrix4 const& lhs, Matrix4 const& rhs)
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

static Matrix4 matrix4_mul(Matrix4 const& lhs, Matrix4 const& rhs)
{
    Matrix4 result = matrix4_zero();

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

static void test_matrix4_mul()
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

    Matrix4 l_to_r = matrix4_mul(lhs, rhs);
    Matrix4 l_to_r_expected(
        105.f, 106.f, 78.f, 53.f,
        124.f, 84.f, 100.f, 65.f,
        132.f, 121.f, 100.f, 47.f,
        155.f, 110.f, 133.f, 65.f
    );

    ASSERT(matrix4_eq(l_to_r_expected, l_to_r));

    Matrix4 r_to_l = matrix4_mul(rhs, lhs);
    Matrix4 r_to_l_expected(
        63.f, 161.f, 123.f, 82.f,
        53.f, 76.f, 57.f, 63.f,
        68.f, 107.f, 69.f, 93.f,
        128.f, 175.f, 117.f, 146.f
    );

    ASSERT(matrix4_eq(r_to_l_expected, r_to_l));
}

static Matrix4 matrix4_translate(Vector3 const& translation)
{
    Matrix4 result = matrix4_identity();
    result.m03 = translation.x;
    result.m13 = translation.y;
    result.m23 = translation.z;
    return result;
}

static Matrix4 matrix4_rotate_LH(Vector3 const& angles_rad)
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

static Matrix4 matrix4_rotate_RH(Vector3 const& angles_rad)
{
    Vector3 flipped_angles = angles_rad;
    flipped_angles.y *= -1;
    return matrix4_rotate_LH(flipped_angles);
}

static Matrix4 matrix4_rotate(Vector3 const& angles_rad)
{
    return matrix4_rotate_RH(angles_rad);
}

static Matrix4 matrix4_perspective_LH(f32 vertical_fov_rad, f32 aspect_ratio, f32 near_z, f32 far_z)
{
    f32 g = 1.0f / tanf(vertical_fov_rad * 0.5f);
    f32 k = far_z / (far_z - near_z);
	
    return Matrix4(
        g / aspect_ratio, 0, 0, 0,
        0, g, 0, 0,
        0, 0, k, -near_z * k,
        0, 0, 1.0f, 0);
}

static Matrix4 matrix4_perspective_RH(f32 vertical_fov_rad, f32 aspect_ratio, f32 near_z, f32 far_z)
{
    f32 g = 1.0f / tanf(vertical_fov_rad * 0.5f);
    f32 k = far_z / (near_z - far_z);
	
    return Matrix4(
        g / aspect_ratio, 0, 0, 0,
        0, g, 0, 0,
        0, 0, k, -(far_z * near_z) / (far_z - near_z),
        0, 0, -1.0f, 0);
}