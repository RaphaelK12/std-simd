/*  This file is part of the Vc library. {{{
Copyright © 2016 Matthias Kretz <kretz@kde.org>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the names of contributing organizations nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

}}}*/

#ifndef VC_DATAPAR_SSE_H_
#define VC_DATAPAR_SSE_H_

#include "macros.h"
#ifdef Vc_HAVE_SSE
#include "storage.h"
#include "x86/intrinsics.h"
#include "x86/convert.h"
#include "x86/arithmetics.h"
#include "maskbool.h"
#include "genericimpl.h"

Vc_VERSIONED_NAMESPACE_BEGIN
namespace detail
{
struct sse_mask_impl;
struct sse_datapar_impl;

template <class T> using sse_datapar_member_type = Storage<T, 16 / sizeof(T)>;
template <class T> using sse_mask_member_type = Storage<T, 16 / sizeof(T)>;

template <class T> struct traits<T, datapar_abi::sse> {
    static_assert(sizeof(T) <= 8,
                  "SSE can only implement operations on element types with sizeof <= 8");
    static constexpr size_t size() noexcept { return 16 / sizeof(T); }

    using datapar_member_type = sse_datapar_member_type<T>;
    using datapar_impl_type = sse_datapar_impl;
    static constexpr size_t datapar_member_alignment = alignof(datapar_member_type);
    using datapar_cast_type = typename datapar_member_type::VectorType;

    using mask_member_type = sse_mask_member_type<T>;
    using mask_impl_type = sse_mask_impl;
    static constexpr size_t mask_member_alignment = alignof(mask_member_type);
    using mask_cast_type = typename mask_member_type::VectorType;
};

template <>
struct traits<long double, datapar_abi::sse>
    : public traits<long double, datapar_abi::scalar> {
};
}  // namespace detail
Vc_VERSIONED_NAMESPACE_END

#ifdef Vc_HAVE_SSE_ABI
Vc_VERSIONED_NAMESPACE_BEGIN
namespace detail
{
// datapar impl {{{1
struct sse_datapar_impl : public generic_datapar_impl<sse_datapar_impl> {
    // member types {{{2
    using abi = datapar_abi::sse;
    template <class T> static constexpr size_t size() { return datapar_size_v<T, abi>; }
    template <class T> using datapar_member_type = sse_datapar_member_type<T>;
    template <class T> using intrinsic_type = typename datapar_member_type<T>::VectorType;
    template <class T> using mask_member_type = sse_mask_member_type<T>;
    template <class T> using datapar = Vc::datapar<T, abi>;
    template <class T> using mask = Vc::mask<T, abi>;
    template <size_t N> using size_tag = std::integral_constant<size_t, N>;
    template <class T> using type_tag = T *;

    // data {{{2
    template <class T> static Vc_INTRINSIC auto data(datapar<T> x) noexcept
    {
        return x.d;
    }

    // broadcast {{{2
    static Vc_INTRINSIC intrinsic_type<float> broadcast(float x, size_tag<4>) noexcept
    {
        return _mm_set1_ps(x);
    }
#ifdef Vc_HAVE_SSE2
    static Vc_INTRINSIC intrinsic_type<double> broadcast(double x, size_tag<2>) noexcept
    {
        return _mm_set1_pd(x);
    }
    template <class T>
    static Vc_INTRINSIC intrinsic_type<T> broadcast(T x, size_tag<2>) noexcept
    {
        return _mm_set1_epi64x(x);
    }
    template <class T>
    static Vc_INTRINSIC intrinsic_type<T> broadcast(T x, size_tag<4>) noexcept
    {
        return _mm_set1_epi32(x);
    }
    template <class T>
    static Vc_INTRINSIC intrinsic_type<T> broadcast(T x, size_tag<8>) noexcept
    {
        return _mm_set1_epi16(x);
    }
    template <class T>
    static Vc_INTRINSIC intrinsic_type<T> broadcast(T x, size_tag<16>) noexcept
    {
        return _mm_set1_epi8(x);
    }
#endif

    // load {{{2
    // from long double has no vector implementation{{{3
    template <class T, class F>
    static Vc_INTRINSIC datapar_member_type<T> load(const long double *mem, F,
                                                    type_tag<T>) noexcept
    {
        return generate_from_n_evaluations<size<T>(), datapar_member_type<T>>(
            [&](auto i) { return static_cast<T>(mem[i]); });
    }

    // load without conversion{{{3
    template <class T, class F>
    static Vc_INTRINSIC intrinsic_type<T> load(const T *mem, F f, type_tag<T>) noexcept
    {
        return detail::load16(mem, f);
    }

    // convert from an SSE load{{{3
    template <class T, class U, class F>
    static Vc_INTRINSIC intrinsic_type<T> load(
        const U *mem, F f, type_tag<T>,
        enable_if<sizeof(T) == sizeof(U)> = nullarg) noexcept
    {
#ifdef Vc_HAVE_FULL_SSE_ABI
        return convert<datapar_member_type<U>, datapar_member_type<T>>(
            detail::load16(mem, f));
#else
        unused(f);
        return generate_from_n_evaluations<size<T>(), intrinsic_type<T>>(
            [&](auto i) { return static_cast<T>(mem[i]); });
#endif
    }

    // convert from a half SSE load{{{3
    template <class T, class U, class F>
    static Vc_INTRINSIC intrinsic_type<T> load(
        const U *mem, F, type_tag<T>,
        enable_if<sizeof(T) == sizeof(U) * 2> = nullarg) noexcept
    {
#ifdef Vc_HAVE_FULL_SSE_ABI
        return convert<datapar_member_type<U>, datapar_member_type<T>>(
            intrin_cast<detail::intrinsic_type<U, size<U>()>>(
                _mm_loadl_epi64(reinterpret_cast<const __m128i *>(mem))));
#else
        return generate_from_n_evaluations<size<T>(), intrinsic_type<T>>(
            [&](auto i) { return static_cast<T>(mem[i]); });
#endif
    }

    // convert from a quarter SSE load{{{3
    template <class T, class U, class F>
    static Vc_INTRINSIC intrinsic_type<T> load(
        const U *mem, F, type_tag<T>,
        enable_if<sizeof(T) == sizeof(U) * 4> = nullarg) noexcept
    {
#ifdef Vc_HAVE_FULL_SSE_ABI
        return convert<datapar_member_type<U>, datapar_member_type<T>>(
            intrin_cast<detail::intrinsic_type<U, size<U>()>>(
                _mm_load_ss(reinterpret_cast<const may_alias<float> *>(mem))));
#else
        return generate_from_n_evaluations<size<T>(), intrinsic_type<T>>(
            [&](auto i) { return static_cast<T>(mem[i]); });
#endif
    }

    // convert from a 1/8th SSE load{{{3
    template <class T, class U, class F>
    static Vc_INTRINSIC intrinsic_type<T> load(
        const U *mem, F, type_tag<T>,
        enable_if<sizeof(T) == sizeof(U) * 8> = nullarg) noexcept
    {
#ifdef Vc_HAVE_FULL_SSE_ABI
        return convert<datapar_member_type<U>, datapar_member_type<T>>(
            intrin_cast<detail::intrinsic_type<U, size<U>()>>(
                _mm_cvtsi32_si128(*reinterpret_cast<const may_alias<uint16_t> *>(mem))));
#else
        return generate_from_n_evaluations<size<T>(), intrinsic_type<T>>(
            [&](auto i) { return static_cast<T>(mem[i]); });
#endif
    }

    // AVX and AVX-512 datapar_member_type aliases{{{3
    template <class T>
    using avx_member_type = typename traits<T, datapar_abi::avx>::datapar_member_type;
    template <class T>
    using avx512_member_type =
        typename traits<T, datapar_abi::avx512>::datapar_member_type;

    // convert from an AVX/2-SSE load{{{3
    template <class T, class U, class F>
    static Vc_INTRINSIC intrinsic_type<T> load(
        const U *mem, F f, type_tag<T>,
        enable_if<sizeof(T) * 2 == sizeof(U)> = nullarg) noexcept
    {
#ifdef Vc_HAVE_AVX
        return convert<avx_member_type<U>, datapar_member_type<T>>(
            detail::load32(mem, f));
#elif defined Vc_HAVE_FULL_SSE_ABI
        return convert<datapar_member_type<U>, datapar_member_type<T>>(
            load(mem, f, type_tag<U>()), load(mem + size<U>(), f, type_tag<U>()));
#else
        unused(f);
        return generate_from_n_evaluations<size<T>(), intrinsic_type<T>>(
            [&](auto i) { return static_cast<T>(mem[i]); });
#endif
    }

    // convert from an AVX512/2-AVX/4-SSE load{{{3
    template <class T, class U, class F>
    static Vc_INTRINSIC intrinsic_type<T> load(
        const U *mem, F f, type_tag<T>,
        enable_if<sizeof(T) * 4 == sizeof(U)> = nullarg) noexcept
    {
#ifdef Vc_HAVE_AVX512F
        return convert<avx512_member_type<U>, datapar_member_type<T>>(load64(mem, f));
#elif defined Vc_HAVE_AVX
        return convert<avx_member_type<U>, datapar_member_type<T>>(
            detail::load32(mem, f), detail::load32(mem + 2 * size<U>(), f));
#else
        return convert<datapar_member_type<U>, datapar_member_type<T>>(
            load(mem, f, type_tag<U>()), load(mem + size<U>(), f, type_tag<U>()),
            load(mem + 2 * size<U>(), f, type_tag<U>()),
            load(mem + 3 * size<U>(), f, type_tag<U>()));
#endif
    }

    // convert from a 2-AVX512/4-AVX/8-SSE load{{{3
    template <class T, class U, class F>
    static Vc_INTRINSIC intrinsic_type<T> load(
        const U *mem, F f, type_tag<T>,
        enable_if<sizeof(T) * 8 == sizeof(U)> = nullarg) noexcept
    {
#ifdef Vc_HAVE_AVX512F
        return convert<avx512_member_type<U>, datapar_member_type<T>>(
            load64(mem, f), load64(mem + 4 * size<U>(), f));
#elif defined Vc_HAVE_AVX
        return convert<avx_member_type<U>, datapar_member_type<T>>(
            load32(mem, f), load32(mem + 2 * size<U>(), f), load32(mem + 4 * size<U>(), f),
            load32(mem + 6 * size<U>(), f));
#else
        return convert<datapar_member_type<U>, datapar_member_type<T>>(
            load16(mem, f), load16(mem + size<U>(), f), load16(mem + 2 * size<U>(), f),
            load16(mem + 3 * size<U>(), f), load16(mem + 4 * size<U>(), f),
            load16(mem + 5 * size<U>(), f), load16(mem + 6 * size<U>(), f),
            load16(mem + 7 * size<U>(), f));
#endif
    }

    // masked load {{{2
    template <class T, class U, class F>
    static Vc_INTRINSIC void masked_load(datapar_member_type<T> &merge, mask<T> k,
                                         const U *mem, F) noexcept
    {
        // TODO: implement with V(P)MASKMOV if AVX(2) is available
        execute_n_times<size<T>()>([&](auto i) {
            if (k.d.m(i)) {
                merge.set(i, static_cast<T>(mem[i]));
            }
        });
    }

    // store {{{2
    // store to long double has no vector implementation{{{3
    template <class T, class F>
    static Vc_INTRINSIC void store(datapar_member_type<T> v, long double *mem, F,
                                   type_tag<T>) noexcept
    {
        // alignment F doesn't matter
        execute_n_times<size<T>()>([&](auto i) { mem[i] = v.m(i); });
    }

    // store without conversion{{{3
    template <class T, class F>
    static Vc_INTRINSIC void store(datapar_member_type<T> v, T *mem, F f,
                                   type_tag<T>) noexcept
    {
        store16(v, mem, f);
    }

    // convert and 16-bit store{{{3
    template <class T, class U, class F>
    static Vc_INTRINSIC void store(datapar_member_type<T> v, U *mem, F f, type_tag<T>,
                                   enable_if<sizeof(T) == sizeof(U) * 8> = nullarg) noexcept
    {
        store2(convert<datapar_member_type<T>, datapar_member_type<U>>(v), mem, f);
    }

    // convert and 32-bit store{{{3
    template <class T, class U, class F>
    static Vc_INTRINSIC void store(datapar_member_type<T> v, U *mem, F f, type_tag<T>,
                                   enable_if<sizeof(T) == sizeof(U) * 4> = nullarg) noexcept
    {
#ifdef Vc_HAVE_FULL_SSE_ABI
        store4(convert<datapar_member_type<T>, datapar_member_type<U>>(v), mem, f);
#else
        unused(f);
        execute_n_times<size<T>()>([&](auto i) { mem[i] = static_cast<U>(v[i]); });
#endif
    }

    // convert and 64-bit store{{{3
    template <class T, class U, class F>
    static Vc_INTRINSIC void store(datapar_member_type<T> v, U *mem, F f, type_tag<T>,
                                   enable_if<sizeof(T) == sizeof(U) * 2> = nullarg) noexcept
    {
#ifdef Vc_HAVE_FULL_SSE_ABI
        store8(convert<datapar_member_type<T>, datapar_member_type<U>>(v), mem, f);
#else
        unused(f);
        execute_n_times<size<T>()>([&](auto i) { mem[i] = static_cast<U>(v[i]); });
#endif
    }

    // convert and 128-bit store{{{3
    template <class T, class U, class F>
    static Vc_INTRINSIC void store(datapar_member_type<T> v, U *mem, F f, type_tag<T>,
                                   enable_if<sizeof(T) == sizeof(U)> = nullarg) noexcept
    {
#ifdef Vc_HAVE_FULL_SSE_ABI
        store16(convert<datapar_member_type<T>, datapar_member_type<U>>(v), mem, f);
#else
        unused(f);
        execute_n_times<size<T>()>([&](auto i) { mem[i] = static_cast<U>(v[i]); });
#endif
    }

    // convert and 256-bit store{{{3
    template <class T, class U, class F>
    static Vc_INTRINSIC void store(
        datapar_member_type<T> v, U *mem, F f, type_tag<T>,
        enable_if<sizeof(T) * 2 == sizeof(U)> = nullarg) noexcept
    {
#ifdef Vc_HAVE_AVX
        store32(convert<datapar_member_type<T>, avx_member_type<U>>(v), mem, f);
#elif defined Vc_HAVE_FULL_SSE_ABI
        // without the full SSE ABI there cannot be any vectorized converting loads
        // because only float vectors exist
        const auto tmp = convert_all<datapar_member_type<U>>(v);
        store16(tmp[0], mem, f);
        store16(tmp[1], mem + size<T>() / 2, f);
#else
        execute_n_times<size<T>()>([&](auto i) { mem[i] = static_cast<U>(v[i]); });
        detail::unused(f);
#endif
    }

    // convert and 512-bit store{{{3
    template <class T, class U, class F>
    static Vc_INTRINSIC void store(
        datapar_member_type<T> v, U *mem, F f, type_tag<T>,
        enable_if<sizeof(T) * 4 == sizeof(U)> = nullarg) noexcept
    {
#ifdef Vc_HAVE_AVX512F
        store64(convert_all<avx512_member_type<U>>(v), mem, f);
#elif defined Vc_HAVE_AVX
        const auto tmp = convert_all<avx_member_type<U>>(v);
        store32(tmp[0], mem, f);
        store32(tmp[1], mem + size<T>() / 2, f);
#else
        const auto tmp = convert_all<datapar_member_type<U>>(v);
        store16(tmp[0], mem, f);
        store16(tmp[1], mem + size<T>() * 1 / 4, f);
        store16(tmp[2], mem + size<T>() * 2 / 4, f);
        store16(tmp[3], mem + size<T>() * 3 / 4, f);
#endif
    }

    // convert and 1024-bit store{{{3
    template <class T, class U, class F>
    static Vc_INTRINSIC void store(
        datapar_member_type<T> v, U *mem, F f, type_tag<T>,
        enable_if<sizeof(T) * 8 == sizeof(U)> = nullarg) noexcept
    {
#ifdef Vc_HAVE_AVX512F
        const auto tmp = convert_all<avx512_member_type<U>>(v);
        store64(tmp[0], mem, f);
        store64(tmp[1], mem + size<T>() / 2, f);
#elif defined Vc_HAVE_AVX
        const auto tmp = convert_all<avx_member_type<U>>(v);
        store32(tmp[0], mem, f);
        store32(tmp[1], mem + size<T>() * 1 / 4, f);
        store32(tmp[2], mem + size<T>() * 2 / 4, f);
        store32(tmp[3], mem + size<T>() * 3 / 4, f);
#else
        const auto tmp = convert_all<datapar_member_type<U>>(v);
        store16(tmp[0], mem, f);
        store16(tmp[1], mem + size<T>() * 1 / 8, f);
        store16(tmp[2], mem + size<T>() * 2 / 8, f);
        store16(tmp[3], mem + size<T>() * 3 / 8, f);
        store16(tmp[4], mem + size<T>() * 4 / 8, f);
        store16(tmp[5], mem + size<T>() * 5 / 8, f);
        store16(tmp[6], mem + size<T>() * 6 / 8, f);
        store16(tmp[7], mem + size<T>() * 7 / 8, f);
#endif
    }

    // masked store {{{2
    template <class T, class F>
    static Vc_INTRINSIC void masked_store(datapar_member_type<T> v, long double *mem, F,
                                          mask<T> k) noexcept
    {
        // no SSE support for long double
        execute_n_times<size<T>()>([&](auto i) {
            if (k.d.m(i)) {
                mem[i] = v.m(i);
            }
        });
    }
    template <class T, class U, class F>
    static Vc_INTRINSIC void masked_store(datapar_member_type<T> v, U *mem, F,
                                          mask<T> k) noexcept
    {
        //TODO: detail::masked_store(mem, v.v(), k.d.v(), f);
        execute_n_times<size<T>()>([&](auto i) {
            if (k.d.m(i)) {
                mem[i] = static_cast<T>(v.m(i));
            }
        });
    }

    // negation {{{2
    template <class T> static Vc_INTRINSIC mask<T> negate(datapar<T> x) noexcept
    {
#if defined Vc_GCC && defined Vc_USE_BUILTIN_VECTOR_TYPES
        return {private_init, !x.d.builtin()};
#else
        return equal_to(x, datapar<T>(0));
#endif
    }

    // compares {{{2
#if defined Vc_USE_BUILTIN_VECTOR_TYPES
    template <class T>
    static Vc_INTRINSIC mask<T> equal_to(datapar<T> x, datapar<T> y)
    {
        return {private_init, x.d.builtin() == y.d.builtin()};
    }
    template <class T>
    static Vc_INTRINSIC mask<T> not_equal_to(datapar<T> x, datapar<T> y)
    {
        return {private_init, x.d.builtin() != y.d.builtin()};
    }
    template <class T>
    static Vc_INTRINSIC mask<T> less(datapar<T> x, datapar<T> y)
    {
        return {private_init, x.d.builtin() < y.d.builtin()};
    }
    template <class T>
    static Vc_INTRINSIC mask<T> less_equal(datapar<T> x, datapar<T> y)
    {
        return {private_init, x.d.builtin() <= y.d.builtin()};
    }
#else
    static Vc_INTRINSIC mask<double> equal_to(datapar<double> x, datapar<double> y) { return {private_init, _mm_cmpeq_pd(x.d, y.d)}; }
    static Vc_INTRINSIC mask< float> equal_to(datapar< float> x, datapar< float> y) { return {private_init, _mm_cmpeq_ps(x.d, y.d)}; }
    static Vc_INTRINSIC mask< llong> equal_to(datapar< llong> x, datapar< llong> y) { return {private_init, cmpeq_epi64(x.d, y.d)}; }
    static Vc_INTRINSIC mask<ullong> equal_to(datapar<ullong> x, datapar<ullong> y) { return {private_init, cmpeq_epi64(x.d, y.d)}; }
    static Vc_INTRINSIC mask<  long> equal_to(datapar<  long> x, datapar<  long> y) { return {private_init, sizeof(long) == 8 ? cmpeq_epi64(x.d, y.d) : _mm_cmpeq_epi32(x.d, y.d)}; }
    static Vc_INTRINSIC mask< ulong> equal_to(datapar< ulong> x, datapar< ulong> y) { return {private_init, sizeof(long) == 8 ? cmpeq_epi64(x.d, y.d) : _mm_cmpeq_epi32(x.d, y.d)}; }
    static Vc_INTRINSIC mask<   int> equal_to(datapar<   int> x, datapar<   int> y) { return {private_init, _mm_cmpeq_epi32(x.d, y.d)}; }
    static Vc_INTRINSIC mask<  uint> equal_to(datapar<  uint> x, datapar<  uint> y) { return {private_init, _mm_cmpeq_epi32(x.d, y.d)}; }
    static Vc_INTRINSIC mask< short> equal_to(datapar< short> x, datapar< short> y) { return {private_init, _mm_cmpeq_epi16(x.d, y.d)}; }
    static Vc_INTRINSIC mask<ushort> equal_to(datapar<ushort> x, datapar<ushort> y) { return {private_init, _mm_cmpeq_epi16(x.d, y.d)}; }
    static Vc_INTRINSIC mask< schar> equal_to(datapar< schar> x, datapar< schar> y) { return {private_init, _mm_cmpeq_epi8(x.d, y.d)}; }
    static Vc_INTRINSIC mask< uchar> equal_to(datapar< uchar> x, datapar< uchar> y) { return {private_init, _mm_cmpeq_epi8(x.d, y.d)}; }

    static Vc_INTRINSIC mask<double> not_equal_to(datapar<double> x, datapar<double> y) { return {private_init, _mm_cmpneq_pd(x.d, y.d)}; }
    static Vc_INTRINSIC mask< float> not_equal_to(datapar< float> x, datapar< float> y) { return {private_init, _mm_cmpneq_ps(x.d, y.d)}; }
    static Vc_INTRINSIC mask< llong> not_equal_to(datapar< llong> x, datapar< llong> y) { return {private_init, detail::not_(cmpeq_epi64(x.d, y.d))}; }
    static Vc_INTRINSIC mask<ullong> not_equal_to(datapar<ullong> x, datapar<ullong> y) { return {private_init, detail::not_(cmpeq_epi64(x.d, y.d))}; }
    static Vc_INTRINSIC mask<  long> not_equal_to(datapar<  long> x, datapar<  long> y) { return {private_init, detail::not_(sizeof(long) == 8 ? cmpeq_epi64(x.d, y.d) : _mm_cmpeq_epi32(x.d, y.d))}; }
    static Vc_INTRINSIC mask< ulong> not_equal_to(datapar< ulong> x, datapar< ulong> y) { return {private_init, detail::not_(sizeof(long) == 8 ? cmpeq_epi64(x.d, y.d) : _mm_cmpeq_epi32(x.d, y.d))}; }
    static Vc_INTRINSIC mask<   int> not_equal_to(datapar<   int> x, datapar<   int> y) { return {private_init, detail::not_(_mm_cmpeq_epi32(x.d, y.d))}; }
    static Vc_INTRINSIC mask<  uint> not_equal_to(datapar<  uint> x, datapar<  uint> y) { return {private_init, detail::not_(_mm_cmpeq_epi32(x.d, y.d))}; }
    static Vc_INTRINSIC mask< short> not_equal_to(datapar< short> x, datapar< short> y) { return {private_init, detail::not_(_mm_cmpeq_epi16(x.d, y.d))}; }
    static Vc_INTRINSIC mask<ushort> not_equal_to(datapar<ushort> x, datapar<ushort> y) { return {private_init, detail::not_(_mm_cmpeq_epi16(x.d, y.d))}; }
    static Vc_INTRINSIC mask< schar> not_equal_to(datapar< schar> x, datapar< schar> y) { return {private_init, detail::not_(_mm_cmpeq_epi8(x.d, y.d))}; }
    static Vc_INTRINSIC mask< uchar> not_equal_to(datapar< uchar> x, datapar< uchar> y) { return {private_init, detail::not_(_mm_cmpeq_epi8(x.d, y.d))}; }

    static Vc_INTRINSIC mask<double> less(datapar<double> x, datapar<double> y) { return {private_init, _mm_cmplt_pd(x.d, y.d)}; }
    static Vc_INTRINSIC mask< float> less(datapar< float> x, datapar< float> y) { return {private_init, _mm_cmplt_ps(x.d, y.d)}; }
    static Vc_INTRINSIC mask< llong> less(datapar< llong> x, datapar< llong> y) { return {private_init, cmpgt_epi64(y.d, x.d)}; }
    static Vc_INTRINSIC mask<ullong> less(datapar<ullong> x, datapar<ullong> y) { return {private_init, cmpgt_epu64(y.d, x.d)}; }
    static Vc_INTRINSIC mask<  long> less(datapar<  long> x, datapar<  long> y) { return {private_init, sizeof(long) == 8 ? cmpgt_epi64(y.d, x.d) :  _mm_cmpgt_epi32(y.d, x.d)}; }
    static Vc_INTRINSIC mask< ulong> less(datapar< ulong> x, datapar< ulong> y) { return {private_init, sizeof(long) == 8 ? cmpgt_epu64(y.d, x.d) : cmpgt_epu32(y.d, x.d)}; }
    static Vc_INTRINSIC mask<   int> less(datapar<   int> x, datapar<   int> y) { return {private_init,  _mm_cmpgt_epi32(y.d, x.d)}; }
    static Vc_INTRINSIC mask<  uint> less(datapar<  uint> x, datapar<  uint> y) { return {private_init, cmpgt_epu32(y.d, x.d)}; }
    static Vc_INTRINSIC mask< short> less(datapar< short> x, datapar< short> y) { return {private_init,  _mm_cmpgt_epi16(y.d, x.d)}; }
    static Vc_INTRINSIC mask<ushort> less(datapar<ushort> x, datapar<ushort> y) { return {private_init, cmpgt_epu16(y.d, x.d)}; }
    static Vc_INTRINSIC mask< schar> less(datapar< schar> x, datapar< schar> y) { return {private_init,  _mm_cmpgt_epi8 (y.d, x.d)}; }
    static Vc_INTRINSIC mask< uchar> less(datapar< uchar> x, datapar< uchar> y) { return {private_init, cmpgt_epu8 (y.d, x.d)}; }

    static Vc_INTRINSIC mask<double> less_equal(datapar<double> x, datapar<double> y) { return {private_init, _mm_cmple_pd(x.d, y.d)}; }
    static Vc_INTRINSIC mask< float> less_equal(datapar< float> x, datapar< float> y) { return {private_init, _mm_cmple_ps(x.d, y.d)}; }
    static Vc_INTRINSIC mask< llong> less_equal(datapar< llong> x, datapar< llong> y) { return {private_init, detail::not_(cmpgt_epi64(x.d, y.d))}; }
    static Vc_INTRINSIC mask<ullong> less_equal(datapar<ullong> x, datapar<ullong> y) { return {private_init, detail::not_(cmpgt_epu64(x.d, y.d))}; }
    static Vc_INTRINSIC mask<  long> less_equal(datapar<  long> x, datapar<  long> y) { return {private_init, detail::not_(sizeof(long) == 8 ? cmpgt_epi64(x.d, y.d) :  _mm_cmpgt_epi32(x.d, y.d))}; }
    static Vc_INTRINSIC mask< ulong> less_equal(datapar< ulong> x, datapar< ulong> y) { return {private_init, detail::not_(sizeof(long) == 8 ? cmpgt_epu64(x.d, y.d) : cmpgt_epu32(x.d, y.d))}; }
    static Vc_INTRINSIC mask<   int> less_equal(datapar<   int> x, datapar<   int> y) { return {private_init, detail::not_( _mm_cmpgt_epi32(x.d, y.d))}; }
    static Vc_INTRINSIC mask<  uint> less_equal(datapar<  uint> x, datapar<  uint> y) { return {private_init, detail::not_(cmpgt_epu32(x.d, y.d))}; }
    static Vc_INTRINSIC mask< short> less_equal(datapar< short> x, datapar< short> y) { return {private_init, detail::not_( _mm_cmpgt_epi16(x.d, y.d))}; }
    static Vc_INTRINSIC mask<ushort> less_equal(datapar<ushort> x, datapar<ushort> y) { return {private_init, detail::not_(cmpgt_epu16(x.d, y.d))}; }
    static Vc_INTRINSIC mask< schar> less_equal(datapar< schar> x, datapar< schar> y) { return {private_init, detail::not_( _mm_cmpgt_epi8 (x.d, y.d))}; }
    static Vc_INTRINSIC mask< uchar> less_equal(datapar< uchar> x, datapar< uchar> y) { return {private_init, detail::not_(cmpgt_epu8 (x.d, y.d))}; }
#endif

    // smart_reference access {{{2
    template <class T, class A>
    static Vc_INTRINSIC T get(Vc::datapar<T, A> v, int i) noexcept
    {
        return v.d.m(i);
    }
    template <class T, class A, class U>
    static Vc_INTRINSIC void set(Vc::datapar<T, A> &v, int i, U &&x) noexcept
    {
        v.d.set(i, std::forward<U>(x));
    }
    // }}}2
};

// mask impl {{{1
struct sse_mask_impl {
    // member types {{{2
    using abi = datapar_abi::sse;
    template <class T> static constexpr size_t size() { return datapar_size_v<T, abi>; }
    template <class T> using mask_member_type = sse_mask_member_type<T>;
    template <class T> using mask = Vc::mask<T, datapar_abi::sse>;
    template <class T> using mask_bool = MaskBool<sizeof(T)>;
    template <size_t N> using size_tag = std::integral_constant<size_t, N>;
    template <class T> using type_tag = T *;

    // broadcast {{{2
    template <class T> static Vc_INTRINSIC auto broadcast(bool x, type_tag<T>) noexcept
    {
        return detail::broadcast16(T(mask_bool<T>{x}));
    }

    // load {{{2
    template <class F>
    static Vc_INTRINSIC auto load(const bool *mem, F, size_tag<4>) noexcept
    {
#ifdef Vc_HAVE_SSE2
        __m128i k = _mm_cvtsi32_si128(*reinterpret_cast<const int *>(mem));
        k = _mm_cmpgt_epi16(_mm_unpacklo_epi8(k, k), _mm_setzero_si128());
        return intrin_cast<__m128>(_mm_unpacklo_epi16(k, k));
#elif defined Vc_HAVE_MMX
        __m128 k = _mm_cvtpi8_ps(_mm_cvtsi32_si64(*reinterpret_cast<const int *>(mem)));
        _mm_empty();
        return _mm_cmpgt_ps(k, detail::zero<__m128>());
#endif  // Vc_HAVE_SSE2
    }
#ifdef Vc_HAVE_SSE2
    template <class F>
    static Vc_INTRINSIC auto load(const bool *mem, F, size_tag<2>) noexcept
    {
        return _mm_set_epi32(-int(mem[1]), -int(mem[1]), -int(mem[0]), -int(mem[0]));
    }
    template <class F>
    static Vc_INTRINSIC auto load(const bool *mem, F, size_tag<8>) noexcept
    {
#ifdef Vc_IS_AMD64
        __m128i k = _mm_cvtsi64_si128(*reinterpret_cast<const int64_t *>(mem));
#else
        __m128i k = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(mem));
#endif
        return intrin_cast<__m128>(
            _mm_cmpgt_epi16(_mm_unpacklo_epi8(k, k), _mm_setzero_si128()));
    }
    template <class F>
    static Vc_INTRINSIC auto load(const bool *mem, F, size_tag<16>) noexcept
    {
        return intrin_cast<__m128>(
            _mm_cmpgt_epi8(std::is_same<F, flags::vector_aligned_tag>::value
                               ? _mm_load_si128(reinterpret_cast<const __m128i *>(mem))
                               : _mm_loadu_si128(reinterpret_cast<const __m128i *>(mem)),
                           _mm_setzero_si128()));
    }
#endif  // Vc_HAVE_SSE2

    // masked load {{{2
    template <class T, class F, class SizeTag>
    static Vc_INTRINSIC void masked_load(mask_member_type<T> &merge,
                                         mask_member_type<T> mask, const bool *mem, F,
                                         SizeTag s) noexcept
    {
        for (std::size_t i = 0; i < s; ++i) {
            if (mask.m(i)) {
                merge.set(i, mask_bool<T>{mem[i]});
            }
        }
    }

    // store {{{2
#ifdef Vc_HAVE_MMX
    template <class F>
    static Vc_INTRINSIC void store(mask_member_type<float> v, bool *mem, F,
                                   size_tag<4>) noexcept
    {
        const __m128 k(v);
        const __m64 kk = _mm_cvtps_pi8(and_(k, detail::one16(float())));
        *reinterpret_cast<may_alias<int32_t> *>(mem) = _mm_cvtsi64_si32(kk);
        _mm_empty();
    }
#endif  // Vc_HAVE_MMX
#ifdef Vc_HAVE_SSE2
    template <class T, class F>
    static Vc_INTRINSIC void store(mask_member_type<T> v, bool *mem, F,
                                   size_tag<2>) noexcept
    {
        const auto k = intrin_cast<__m128i>(v.v());
        mem[0] = -extract_epi32<1>(k);
        mem[1] = -extract_epi32<3>(k);
    }
    template <class T, class F>
    static Vc_INTRINSIC void store(mask_member_type<T> v, bool *mem, F,
                                   size_tag<4>) noexcept
    {
        const auto k = intrin_cast<__m128i>(v.v());
        *reinterpret_cast<may_alias<int32_t> *>(mem) = _mm_cvtsi128_si32(
            _mm_packs_epi16(_mm_srli_epi16(_mm_packs_epi32(k, _mm_setzero_si128()), 15),
                            _mm_setzero_si128()));
    }
    template <class T, class F>
    static Vc_INTRINSIC void store(mask_member_type<T> v, bool *mem, F,
                                   size_tag<8>) noexcept
    {
        auto k = intrin_cast<__m128i>(v.v());
        k = _mm_srli_epi16(k, 15);
        const auto k2 = _mm_packs_epi16(k, _mm_setzero_si128());
#ifdef Vc_IS_AMD64
        *reinterpret_cast<may_alias<int64_t> *>(mem) = _mm_cvtsi128_si64(k2);
#else
        _mm_store_sd(reinterpret_cast<may_alias<double> *>(mem), _mm_castsi128_pd(k2));
#endif
    }
    template <class T, class F>
    static Vc_INTRINSIC void store(mask_member_type<T> v, bool *mem, F,
                                   size_tag<16>) noexcept
    {
        auto k = intrin_cast<__m128i>(v.v());
        k = _mm_and_si128(k, _mm_set1_epi32(0x01010101));
        if (std::is_same<F, flags::vector_aligned_tag>::value) {
            _mm_store_si128(reinterpret_cast<__m128i *>(mem), k);
        } else {
            _mm_storeu_si128(reinterpret_cast<__m128i *>(mem), k);
        }
    }
#endif  // Vc_HAVE_SSE2

    // masked store {{{2
    template <class T, class F, class SizeTag>
    static Vc_INTRINSIC void masked_store(mask_member_type<T> v, bool *mem, F,
                                          mask_member_type<T> k, SizeTag) noexcept
    {
        for (std::size_t i = 0; i < size<T>(); ++i) {
            if (k.m(i)) {
                mem[i] = v.m(i);
            }
        }
    }

    // negation {{{2
    template <class T, class SizeTag>
    static Vc_INTRINSIC mask_member_type<T> negate(const mask_member_type<T> &x,
                                                   SizeTag) noexcept
    {
#if defined Vc_GCC && defined Vc_USE_BUILTIN_VECTOR_TYPES
        return !x.builtin();
#else
        return detail::not_(x.v());
#endif
    }

    // logical and bitwise operators {{{2
    template <class T>
    static Vc_INTRINSIC mask<T> logical_and(const mask<T> &x, const mask<T> &y)
    {
        return {private_init, detail::and_(x.d, y.d)};
    }

    template <class T>
    static Vc_INTRINSIC mask<T> logical_or(const mask<T> &x, const mask<T> &y)
    {
        return {private_init, detail::or_(x.d, y.d)};
    }

    template <class T>
    static Vc_INTRINSIC mask<T> bit_and(const mask<T> &x, const mask<T> &y)
    {
        return {private_init, detail::and_(x.d, y.d)};
    }

    template <class T>
    static Vc_INTRINSIC mask<T> bit_or(const mask<T> &x, const mask<T> &y)
    {
        return {private_init, detail::or_(x.d, y.d)};
    }

    template <class T>
    static Vc_INTRINSIC mask<T> bit_xor(const mask<T> &x, const mask<T> &y)
    {
        return {private_init, detail::xor_(x.d, y.d)};
    }

    // smart_reference access {{{2
    template <class T> static bool get(const mask<T> &k, int i) noexcept
    {
        return k.d.m(i);
    }
    template <class T> static void set(mask<T> &k, int i, bool x) noexcept
    {
        k.d.set(i, mask_bool<T>(x));
    }
    // }}}2
};

// mask compare base {{{1
struct sse_compare_base {
protected:
    template <class T> using V = Vc::datapar<T, Vc::datapar_abi::sse>;
    template <class T> using M = Vc::mask<T, Vc::datapar_abi::sse>;
    template <class T>
    using S = typename Vc::detail::traits<T, Vc::datapar_abi::sse>::mask_cast_type;
};
// }}}1
}  // namespace detail
Vc_VERSIONED_NAMESPACE_END

// [mask.reductions] {{{
Vc_VERSIONED_NAMESPACE_BEGIN
Vc_ALWAYS_INLINE bool all_of(mask<float, datapar_abi::sse> k)
{
    const __m128 d(k);
#if defined Vc_USE_PTEST && defined Vc_HAVE_AVX
    return _mm_testc_ps(d, detail::allone<__m128>());
#elif defined Vc_USE_PTEST
    const auto dd = detail::intrin_cast<__m128i>(d);
    return _mm_testc_si128(dd, detail::allone<__m128i>());
#else
    return _mm_movemask_ps(d) == 0xf;
#endif
}

Vc_ALWAYS_INLINE bool any_of(mask<float, datapar_abi::sse> k)
{
    const __m128 d(k);
#if defined Vc_USE_PTEST && defined Vc_HAVE_AVX
    return 0 == _mm_testz_ps(d, d);
#elif defined Vc_USE_PTEST
    const auto dd = detail::intrin_cast<__m128i>(d);
    return 0 == _mm_testz_si128(dd, dd);
#else
    return _mm_movemask_ps(d) != 0;
#endif
}

Vc_ALWAYS_INLINE bool none_of(mask<float, datapar_abi::sse> k)
{
    const __m128 d(k);
#if defined Vc_USE_PTEST && defined Vc_HAVE_AVX
    return 0 != _mm_testz_ps(d, d);
#elif defined Vc_USE_PTEST
    const auto dd = detail::intrin_cast<__m128i>(d);
    return 0 != _mm_testz_si128(dd, dd);
#else
    return _mm_movemask_ps(d) == 0;
#endif
}

Vc_ALWAYS_INLINE bool some_of(mask<float, datapar_abi::sse> k)
{
    const __m128 d(k);
#if defined Vc_USE_PTEST && defined Vc_HAVE_AVX
    return _mm_testnzc_ps(d, detail::allone<__m128>());
#elif defined Vc_USE_PTEST
    const auto dd = detail::intrin_cast<__m128i>(d);
    return _mm_testnzc_si128(dd, detail::allone<__m128i>());
#else
    const int tmp = _mm_movemask_ps(d);
    return tmp != 0 && (tmp ^ 0xf) != 0;
#endif
}

#ifdef Vc_HAVE_SSE2
Vc_ALWAYS_INLINE bool all_of(mask<double, datapar_abi::sse> k)
{
    __m128d d(k);
#ifdef Vc_USE_PTEST
#ifdef Vc_HAVE_AVX
    return _mm_testc_pd(d, detail::allone<__m128d>());
#else
    const auto dd = detail::intrin_cast<__m128i>(d);
    return _mm_testc_si128(dd, detail::allone<__m128i>());
#endif
#else
    return _mm_movemask_pd(d) == 0x3;
#endif
}

Vc_ALWAYS_INLINE bool any_of(mask<double, datapar_abi::sse> k)
{
    const __m128d d(k);
#if defined Vc_USE_PTEST && defined Vc_HAVE_AVX
    return 0 == _mm_testz_pd(d, d);
#elif defined Vc_USE_PTEST
    const auto dd = detail::intrin_cast<__m128i>(d);
    return 0 == _mm_testz_si128(dd, dd);
#else
    return _mm_movemask_pd(d) != 0;
#endif
}

Vc_ALWAYS_INLINE bool none_of(mask<double, datapar_abi::sse> k)
{
    const __m128d d(k);
#if defined Vc_USE_PTEST && defined Vc_HAVE_AVX
    return 0 != _mm_testz_pd(d, d);
#elif defined Vc_USE_PTEST
    const auto dd = detail::intrin_cast<__m128i>(d);
    return 0 != _mm_testz_si128(dd, dd);
#else
    return _mm_movemask_pd(d) == 0;
#endif
}

Vc_ALWAYS_INLINE bool some_of(mask<double, datapar_abi::sse> k)
{
    const __m128d d(k);
#if defined Vc_USE_PTEST && defined Vc_HAVE_AVX
    return _mm_testnzc_pd(d, detail::allone<__m128d>());
#elif defined Vc_USE_PTEST
    const auto dd = detail::intrin_cast<__m128i>(d);
    return _mm_testnzc_si128(dd, detail::allone<__m128i>());
#else
    const int tmp = _mm_movemask_pd(d);
    return tmp == 1 || tmp == 2;
#endif
}

template <class T> Vc_ALWAYS_INLINE bool all_of(mask<T, datapar_abi::sse> k)
{
    const __m128i d(k);
#ifdef Vc_USE_PTEST
    return _mm_testc_si128(d, detail::allone<__m128i>());  // return 1 if (0xffffffff,
                                                           // 0xffffffff, 0xffffffff,
                                                           // 0xffffffff) == (~0 & d.v())
#else
    return _mm_movemask_epi8(d) == 0xffff;
#endif
}

template <class T> Vc_ALWAYS_INLINE bool any_of(mask<T, datapar_abi::sse> k)
{
    const __m128i d(k);
#ifdef Vc_USE_PTEST
    return 0 == _mm_testz_si128(d, d);  // return 1 if (0, 0, 0, 0) == (d.v() & d.v())
#else
    return _mm_movemask_epi8(d) != 0x0000;
#endif
}

template <class T> Vc_ALWAYS_INLINE bool none_of(mask<T, datapar_abi::sse> k)
{
    const __m128i d(k);
#ifdef Vc_USE_PTEST
    return 0 != _mm_testz_si128(d, d);  // return 1 if (0, 0, 0, 0) == (d.v() & d.v())
#else
    return _mm_movemask_epi8(d) == 0x0000;
#endif
}

template <class T> Vc_ALWAYS_INLINE bool some_of(mask<T, datapar_abi::sse> k)
{
    const __m128i d(k);
#ifdef Vc_USE_PTEST
    return _mm_test_mix_ones_zeros(d, detail::allone<__m128i>());
#else
    const int tmp = _mm_movemask_epi8(d);
    return tmp != 0 && (tmp ^ 0xffff) != 0;
#endif
}
#endif

template <class T> Vc_ALWAYS_INLINE int popcount(mask<T, datapar_abi::sse> k)
{
    const auto d =
        static_cast<typename detail::traits<T, datapar_abi::sse>::mask_cast_type>(k);
    return detail::mask_count<k.size()>(d);
}

template <class T> Vc_ALWAYS_INLINE int find_first_set(mask<T, datapar_abi::sse> k)
{
    const auto d =
        static_cast<typename detail::traits<T, datapar_abi::sse>::mask_cast_type>(k);
    return detail::firstbit(detail::mask_to_int<k.size()>(d));
}

template <class T> Vc_ALWAYS_INLINE int find_last_set(mask<T, datapar_abi::sse> k)
{
    const auto d =
        static_cast<typename detail::traits<T, datapar_abi::sse>::mask_cast_type>(k);
    return detail::lastbit(detail::mask_to_int<k.size()>(d));
}

Vc_ALWAYS_INLINE bool all_of(mask<long double, datapar_abi::sse> k) { return all_of(k[0]); }
Vc_ALWAYS_INLINE bool any_of(mask<long double, datapar_abi::sse> k) { return any_of(k[0]); }
Vc_ALWAYS_INLINE bool none_of(mask<long double, datapar_abi::sse> k) { return none_of(k[0]); }
Vc_ALWAYS_INLINE bool some_of(mask<long double, datapar_abi::sse> k) { return some_of(k[0]); }
Vc_ALWAYS_INLINE int popcount(mask<long double, datapar_abi::sse> k) { return popcount(k[0]); }
Vc_ALWAYS_INLINE int find_first_set(mask<long double, datapar_abi::sse> k) { return find_first_set(k[0]); }
Vc_ALWAYS_INLINE int find_last_set(mask<long double, datapar_abi::sse> k) { return find_last_set(k[0]); }

Vc_VERSIONED_NAMESPACE_END
// }}}

namespace std
{
// mask operators {{{1
template <class T>
struct equal_to<Vc::mask<T, Vc::datapar_abi::sse>>
    : private Vc::detail::sse_compare_base {
public:
    bool operator()(const M<T> &x, const M<T> &y) const noexcept
    {
        return Vc::detail::is_equal<M<T>::size()>(static_cast<S<T>>(x),
                                                  static_cast<S<T>>(y));
    }
};
template <>
struct equal_to<Vc::mask<long double, Vc::datapar_abi::sse>>
    : public equal_to<Vc::mask<long double, Vc::datapar_abi::scalar>> {
};
// }}}1
}  // namespace std
#endif  // Vc_HAVE_SSE_ABI
#endif  // Vc_HAVE_SSE

#endif  // VC_DATAPAR_SSE_H_

// vim: foldmethod=marker
