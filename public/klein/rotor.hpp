#pragma once

#include "detail/matrix.hpp"
#include "direction.hpp"
#include "line.hpp"
#include "mat4x4.hpp"
#include "plane.hpp"
#include "point.hpp"
#include <cmath>

namespace kln
{
/// \defgroup rotor Rotors
///
/// The rotor is an entity that represents a rigid rotation about an axis.
/// To apply the rotor to a supported entity, the call operator is available.
///
/// !!! example
///
///     ```c++
///         // Initialize a point at (1, 3, 2)
///         kln::point p{1.f, 3.f, 2.f};
///
///         // Create a normalized rotor representing a pi/2 radian
///         // rotation about the xz-axis.
///         kln::rotor r{M_PI * 0.5f, 1.f, 0.f, 1.f};
///
///         // Rotate our point using the created rotor
///         kln::point rotated = r(p);
///     ```
///     We can rotate lines and planes as well using the rotor's call operator.
///
/// Rotors can be multiplied to one another with the `*` operator to create
/// a new rotor equivalent to the application of each factor.
///
/// !!! example
///
///     ```c++
///         // Create a normalized rotor representing a $\frac{\pi}{2}$ radian
///         // rotation about the xz-axis.
///         kln::rotor r1{M_PI * 0.5f, 1.f, 0.f, 1.f};
///
///         // Create a second rotor representing a $\frac{\pi}{3}$ radian
///         // rotation about the yz-axis.
///         kln::rotor r2{M_PI / 3.f, 0.f, 1.f, 1.f};
///
///         // Use the geometric product to create a rotor equivalent to first
///         // applying r1, then applying r2. Note that the order of the
///         // operands here is significant.
///         kln::rotor r3 = r2 * r1;
///     ```
///
/// The same `*` operator can be used to compose the rotor's action with other
/// translators and motors.
/// \addtogroup rotor
/// @{
class rotor final
{
public:
    rotor() noexcept = default;

    /// Convenience constructor. Computes transcendentals and normalizes
    /// rotation axis.
    rotor(float ang_rad, float x, float y, float z) noexcept
    {
        float norm     = std::sqrt(x * x + y * y + z * z);
        float inv_norm = -1.f / norm;

        float half = 0.5f * ang_rad;
        // Rely on compiler to coalesce these two assignments into a single
        // sincos call at instruction selection time
        float sin_ang = std::sin(half);
        float scale   = sin_ang * inv_norm;
        p1_           = _mm_set_ps(z, y, x, std::cos(half));
        p1_           = _mm_mul_ps(p1_, _mm_set_ps(scale, scale, scale, 1.f));
    }

    rotor(__m128 p1) noexcept
        : p1_{p1}
    {}

    /// Fast load operation for packed data that is already normalized. The
    /// argument `data` should point to a set of 4 float values with layout `(a,
    /// b, c, d)` corresponding to the multivector
    /// $a + b\mathbf{e}_{23} + c\mathbf{e}_{31} + d\mathbf{e}_{12}$.
    ///
    /// !!! danger
    ///
    ///     The rotor data loaded this way *must* be normalized. That is, the
    ///     rotor $r$ must satisfy $r\widetilde{r} = 1$.
    void load_normalized(float* data) noexcept
    {
        p1_ = _mm_loadu_ps(data);
    }

    /// Normalize a rotor such that $\mathbf{r}\widetilde{\mathbf{r}} = 1$.
    ///
    /// !!! tip
    ///
    ///     Normalization here is done using the `rsqrtps`
    ///     instruction with a maximum relative error of $1.5\times 2^{-12}$.
    void normalize() noexcept
    {
        // A rotor is normalized if r * ~r is unity.
        __m128 inv_norm = _mm_rsqrt_ps(detail::dp_bc(p1_, p1_));
        p1_             = _mm_mul_ps(p1_, inv_norm);
    }

    /// Return a normalized copy of this rotor
    [[nodiscard]] rotor normalized() const noexcept
    {
        rotor out = *this;
        out.normalize();
        return out;
    }

    /// Converts the rotor to a 3x4 column-major matrix. The results of this
    /// conversion are only defined if the rotor is normalized, and this
    /// conversion is preferable if so.
    [[nodiscard]] mat3x4 as_mat3x4() const noexcept
    {
        mat3x4 out;
        mat4x4_12<false, true>(p1_, nullptr, out.cols);
        return out;
    }

    /// Converts the rotor to a 4x4 column-major matrix.
    [[nodiscard]] mat4x4 as_mat4x4() const noexcept
    {
        mat4x4 out;
        mat4x4_12<false>(p1_, nullptr, out.cols);
        return out;
    }

    /// Conjugates a plane $p$ with this rotor and returns the result
    /// $rp\widetilde{r}$.
    [[nodiscard]] plane KLN_VEC_CALL operator()(plane const& p) const noexcept
    {
        plane out;
        detail::sw012<false, false>(&p.p0_, p1_, nullptr, &out.p0_);
        return out;
    }

    /// Conjugates an array of planes with this rotor in the input array and
    /// stores the result in the output array. Aliasing is only permitted when
    /// `in == out` (in place motor application).
    ///
    /// !!! tip
    ///
    ///     When applying a rotor to a list of tightly packed planes, this
    ///     routine will be *significantly faster* than applying the rotor to
    ///     each plane individually.
    void KLN_VEC_CALL operator()(plane* in, plane* out, size_t count) const
        noexcept
    {
        detail::sw012<true, false>(&in->p0_, p1_, nullptr, &out->p0_, count);
    }

    [[nodiscard]] branch KLN_VEC_CALL operator()(branch const& b) const noexcept
    {
        branch out;
        detail::swMM<false, false, false>(&b.p1_, p1_, nullptr, &out.p1_);
        return out;
    }

    /// Conjugates a line $\ell$ with this rotor and returns the result
    /// $r\ell \widetilde{r}$.
    [[nodiscard]] line KLN_VEC_CALL operator()(line const& l) const noexcept
    {
        line out;
        detail::swMM<false, false, true>(&l.p1_, p1_, nullptr, &out.p1_);
        return out;
    }

    /// Conjugates an array of lines with this rotor in the input array and
    /// stores the result in the output array. Aliasing is only permitted when
    /// `in == out` (in place rotor application).
    ///
    /// !!! tip
    ///
    ///     When applying a rotor to a list of tightly packed lines, this
    ///     routine will be *significantly faster* than applying the rotor to
    ///     each line individually.
    void KLN_VEC_CALL operator()(line* in, line* out, size_t count) const noexcept
    {
        detail::swMM<true, false, true>(&in->p1_, p1_, nullptr, &out->p1_, count);
    }

    /// Conjugates a point $p$ with this rotor and returns the result
    /// $rp\widetilde{r}$.
    [[nodiscard]] point KLN_VEC_CALL operator()(point const& p) const noexcept
    {
        // NOTE: Conjugation of a plane and point with a rotor is identical
        point out;
        detail::sw012<false, false>(&p.p3_, p1_, nullptr, &out.p3_);
        return out;
    }

    /// Conjugates an array of points with this rotor in the input array and
    /// stores the result in the output array. Aliasing is only permitted when
    /// `in == out` (in place rotor application).
    ///
    /// !!! tip
    ///
    ///     When applying a rotor to a list of tightly packed points, this
    ///     routine will be *significantly faster* than applying the rotor to
    ///     each point individually.
    void KLN_VEC_CALL operator()(point* in, point* out, size_t count) const
        noexcept
    {
        // NOTE: Conjugation of a plane and point with a rotor is identical
        detail::sw012<true, false>(&in->p3_, p1_, nullptr, &out->p3_, count);
    }

    /// Conjugates a direction $d$ with this rotor and returns the result
    /// $rd\widetilde{r}$.
    [[nodiscard]] direction KLN_VEC_CALL operator()(direction const& d) const
        noexcept
    {
        direction out;
        // NOTE: Conjugation of a plane and point with a rotor is identical
        detail::sw012<false, false>(&d.p3_, p1_, nullptr, &out.p3_);
        return out;
    }

    /// Conjugates an array of directions with this rotor in the input array and
    /// stores the result in the output array. Aliasing is only permitted when
    /// `in == out` (in place rotor application).
    ///
    /// !!! tip
    ///
    ///     When applying a rotor to a list of tightly packed directions, this
    ///     routine will be *significantly faster* than applying the rotor to
    ///     each direction individually.
    void KLN_VEC_CALL operator()(direction* in, direction* out, size_t count) const
        noexcept
    {
        // NOTE: Conjugation of a plane and point with a rotor is identical
        detail::sw012<true, false>(&in->p3_, p1_, nullptr, &out->p3_, count);
    }

    /// Rotor addition
    rotor& KLN_VEC_CALL operator+=(rotor b) noexcept
    {
        p1_ = _mm_add_ps(p1_, b.p1_);
        return *this;
    }

    /// Rotor subtraction
    rotor& KLN_VEC_CALL operator-=(rotor b) noexcept
    {
        p1_ = _mm_sub_ps(p1_, b.p1_);
        return *this;
    }

    /// Rotor uniform scale
    rotor& operator*=(float s) noexcept
    {
        p1_ = _mm_mul_ps(p1_, _mm_set1_ps(s));
        return *this;
    }

    /// Rotor uniform scale
    rotor& operator*=(int s) noexcept
    {
        p1_ = _mm_mul_ps(p1_, _mm_set1_ps(static_cast<float>(s)));
        return *this;
    }

    /// Rotor uniform inverse scale
    rotor& operator/=(float s) noexcept
    {
        p1_ = _mm_mul_ps(p1_, _mm_rcp_ps(_mm_set1_ps(s)));
        return *this;
    }

    /// Rotor uniform inverse scale
    rotor& operator/=(int s) noexcept
    {
        p1_ = _mm_mul_ps(p1_, _mm_rcp_ps(_mm_set1_ps(static_cast<float>(s))));
        return *this;
    }

    [[nodiscard]] float scalar() const noexcept
    {
        float out;
        _mm_store_ss(&out, p1_);
        return out;
    }

    [[nodiscard]] float e12() const noexcept
    {
        float out[4];
        _mm_store_ps(out, p1_);
        return out[3];
    }

    [[nodiscard]] float e21() const noexcept
    {
        return -e12();
    }

    [[nodiscard]] float e31() const noexcept
    {
        float out[4];
        _mm_store_ps(out, p1_);
        return out[2];
    }

    [[nodiscard]] float e13() const noexcept
    {
        return -e31();
    }

    [[nodiscard]] float e23() const noexcept
    {
        float out[4];
        _mm_store_ps(out, p1_);
        return out[1];
    }

    [[nodiscard]] float e32() const noexcept
    {
        return -e23();
    }

    __m128 p1_;
};

/// Rotor addition
[[nodiscard]] inline rotor KLN_VEC_CALL operator+(rotor a, rotor b) noexcept
{
    rotor c;
    c.p1_ = _mm_add_ps(a.p1_, b.p1_);
    return c;
}

/// Rotor subtraction
[[nodiscard]] inline rotor KLN_VEC_CALL operator-(rotor a, rotor b) noexcept
{
    rotor c;
    c.p1_ = _mm_sub_ps(a.p1_, b.p1_);
    return c;
}

/// Rotor uniform scale
[[nodiscard]] inline rotor KLN_VEC_CALL operator*(rotor r, float s) noexcept
{
    rotor c;
    c.p1_ = _mm_mul_ps(r.p1_, _mm_set1_ps(s));
    return c;
}

/// Rotor uniform scale
[[nodiscard]] inline rotor KLN_VEC_CALL operator*(rotor r, int s) noexcept
{
    return r * static_cast<float>(s);
}

/// Rotor uniform scale
[[nodiscard]] inline rotor KLN_VEC_CALL operator*(float s, rotor r) noexcept
{
    return r * s;
}

/// Rotor uniform scale
[[nodiscard]] inline rotor KLN_VEC_CALL operator*(int s, rotor r) noexcept
{
    return r * static_cast<float>(s);
}

/// Rotor uniform inverse scale
[[nodiscard]] inline rotor KLN_VEC_CALL operator/(rotor r, float s) noexcept
{
    rotor c;
    c.p1_ = _mm_mul_ps(r.p1_, _mm_rcp_ps(_mm_set1_ps(s)));
    return c;
}

/// Rotor uniform inverse scale
[[nodiscard]] inline rotor KLN_VEC_CALL operator/(rotor r, int s) noexcept
{
    return r / static_cast<float>(s);
}

/// Reversion operator
[[nodiscard]] inline rotor operator~(rotor r) noexcept
{
    __m128 flip = _mm_set_ps(-0.f, -0.f, -0.f, 0.f);
    return {_mm_xor_ps(r.p1_, flip)};
}
} // namespace kln
/// @}
