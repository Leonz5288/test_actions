#pragma once

#define FUNC_DECL

#include "context.h"
#if !defined(TC_GPU)
#include <immintrin.h>
#endif

#include <atomic>
#include <numeric>
#include <mutex>
#include <unordered_map>
#include <iostream>

#if !defined(TC_INCLUDED)

#ifdef _WIN64
#define TC_FORCE_INLINE __forceinline
#else
#define TC_FORCE_INLINE inline __attribute__((always_inline))
#endif
#include <cstdio>
#include <string>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <iostream>
#include <array>
#include <vector>
#include <omp.h>

using float32 = float;
using float64 = double;
using int32 = int;
using uint32 = unsigned int;
using uint64 = unsigned long long;
using uint8 = unsigned char;
using uint16 = unsigned short;

#if defined(TLANG_GPU)
#include <cuda_runtime.h>
#undef FUNC_DECL
#define FUNC_DECL __host__ __device__
#endif

#define TC_ASSERT(x) \
  if (!x)            \
    std::cout << "Ln" << __LINE__ << ":" << #x << std::endl;
namespace taichi {
TC_FORCE_INLINE uint32 rand_int() noexcept {
  static unsigned int x = 123456789, y = 362436069, z = 521288629, w = 88675123;
  unsigned int t = x ^ (x << 11);
  x = y;
  y = z;
  z = w;
  return (w = (w ^ (w >> 19)) ^ (t ^ (t >> 8)));
}

TC_FORCE_INLINE uint64 rand_int64() noexcept {
  return ((uint64)rand_int() << 32) + rand_int();
}

template <typename T>
TC_FORCE_INLINE T rand() noexcept;

template <>
TC_FORCE_INLINE float rand<float>() noexcept {
  return rand_int() * (1.0f / 4294967296.0f);
}

template <>
TC_FORCE_INLINE double rand<double>() noexcept {
  return rand_int() * (1.0 / 4294967296.0);
}

template <>
TC_FORCE_INLINE int rand<int>() noexcept {
  return rand_int();
}

template <typename T>
TC_FORCE_INLINE T rand() noexcept;
}  // namespace taichi

#endif

namespace taichi {
namespace Tlang {

template <typename T, typename G>
T union_cast(G g) {
  static_assert(sizeof(T) == sizeof(G), "");
  union {
    T t;
    G g;
  } u;
  u.g = g;
  return u.t;
}

template <typename T, typename G>
T union_cast_different_size(G g) {
  union {
    T t;
    G g;
  } u;
  u.g = g;
  return u.t;
}

#if !defined(TC_HOST) && !defined(TC_GPU)

// Intrinsics wrapper

template <typename T, int dim>
struct vec_helper;

#define DEFINE_VEC_TYPE(T, dim, _type) \
  template <>                          \
  struct vec_helper<T, dim> {          \
    using type = _type;                \
  };

DEFINE_VEC_TYPE(float32, 1, float32);
DEFINE_VEC_TYPE(int32, 1, int32);
DEFINE_VEC_TYPE(float32, 4, __m128);
DEFINE_VEC_TYPE(int32, 4, __m128i);
DEFINE_VEC_TYPE(float32, 8, __m256);
DEFINE_VEC_TYPE(int32, 8, __m256i);
DEFINE_VEC_TYPE(float32, 16, __m512);
DEFINE_VEC_TYPE(int32, 16, __m512i);

template <typename T, int dim>
struct vec;

template <typename T, int dim>
inline vec<T, dim> set1(T);

template <typename T, int dim>
inline vec<T, dim> load(const void *);

template <typename T, int dim>
inline vec<T, dim> gather(const void *, vec<int32, dim>);

template <typename T, int dim>
inline void store(const vec<T, dim> &v, const void *);

template <typename T, int dim>
inline void store(const vec<T, dim> &v, void *, vec<int32, dim>);

template <typename T, int dim>
inline vec<T, dim> load1(const void *addr);

template <typename T, int dim>
struct vec {
  using type = typename vec_helper<T, dim>::type;
  union {
    type v;
    T e[dim];
  };
  vec() = default;
  vec(type v) : v(v) {
  }
  // template <int _dim = dim>
  // vec(const std::enable_if_t<_dim != 1, vec<T, 1>> &scalar) : vec(scalar.v) {
  //}
  template <typename _T = T>
  vec(std::enable_if_t<!std::is_same<_T, type>::value, T> scalar)
      : v(set1<T, dim>(scalar)) {
  }
  TC_FORCE_INLINE vec(std::array<T, dim> v) {
    for (int i = 0; i < dim; i++) {
      element(i) = (T)v[i];
    }
  }
  operator type() const {
    return v;
  }
  T &operator[](int i) {
    return e[i];
  }
  const T &operator[](int i) const {
    return e[i];
  }
  void print() const {
    std::cout << "[";
    for (int j = 0; j < dim; j++) {
      std::cout << element(j) << ", ";
    }
    std::cout << "]" << std::endl;
  }

  // SIMD types
  template <typename T_ = type>
  std::enable_if_t<!std::is_arithmetic<T_>::value, T> &element(int i) {
    return (*this)[i];
  }

  template <typename T_ = type>
  const std::enable_if_t<!std::is_arithmetic<T_>::value, T> &element(
      int i) const {
    return (*this)[i];
  }

  // scalar types
  template <typename T_ = type>
  typename std::enable_if_t<std::is_arithmetic<T_>::value, T> &element(int i) {
    return v;
  }

  template <typename T_ = type>
  const typename std::enable_if_t<std::is_arithmetic<T_>::value, T> &element(
      int i) const {
    return v;
  }

#if !defined(TC_INCLUDED)
  static vec rand() {
    vec ret;
    for (int i = 0; i < dim; i++) {
      ret[i] = taichi::rand<T>();
    }
    return ret;
  }
#endif

  static vec load(T *addr[dim]) {
    vec ret;
    for (int i = 0; i < dim; i++) {
      ret.element(i) = *addr[i];
    }
    return ret;
  }

  static vec load(void *addr) {
    return taichi::Tlang::load<T, dim>(addr);
  }

  static vec load(void *addr, vec<int, dim> offsets) {
    vec ret;
    for (int i = 0; i < dim; i++) {
      ret.d[i] = gather<T, dim>(addr, offsets.d[i]);
    }
    return ret;
  }

  static vec load1(const void *addr) {
    return taichi::Tlang::load1<T, dim>(addr);
  }

  void store(void *addr) const {
    taichi::Tlang::store<T, dim>(v, addr);
  }

  void store(void *addr, vec<int32, dim> offsets) const {
    taichi::Tlang::store<T, dim>(v, addr, offsets);
  }

  void store(T *addr[dim]) const {
    for (int i = 0; i < dim; i++) {
      *addr[i] = element(i);
      // printf("%p %d\n", addr[i], element(i));
    }
  }
};

//*****************************************************************************

using float32x1 = vec<float32, 1>;
using int32x1 = vec<int32, 1>;
using float32x4 = vec<float32, 4>;
using int32x4 = vec<int32, 4>;
using float32x8 = vec<float32, 8>;
using int32x8 = vec<int32, 8>;
using float32x16 = vec<float32, 16>;
using int32x16 = vec<int32, 16>;
//*****************************************************************************

template <typename T, int dim>
TC_FORCE_INLINE T reduce_sum(const vec<T, dim> &v) {
  T ret(0);
  for (int i = 0; i < dim; i++) {
    ret += v.element(i);
  }
  return ret;
}

template <>
TC_FORCE_INLINE float32 reduce_sum(const vec<float32, 8> &v) {
  auto h = __m256(v);
  auto l = union_cast_different_size<__m256>(_mm256_extractf128_ps(v, 1));
  h = h + l;
  auto H = union_cast_different_size<__m128>(h);
  auto s = _mm_hadd_ps(H, H);
  return s[0] + s[1];
}

//*****************************************************************************

template <>
inline int32x1 load<int32, 1>(const void *addr) {
  return *(int32x1 *)(addr);
}

template <>
inline float32x1 load<float32, 1>(const void *addr) {
  return *(float32x1 *)(addr);
}

template <>
inline float32x4 load<float32, 4>(const void *addr) {
  return _mm_loadu_ps((float32 *)addr);
}

template <>
inline vec<int32, 4> load<int32, 4>(const void *addr) {
  return _mm_loadu_si128((__m128i *)addr);
}

template <>
inline float32x8 load<float32, 8>(const void *addr) {
  return _mm256_loadu_ps((float32 *)addr);
}

template <>
inline vec<int32, 8> load<int32, 8>(const void *addr) {
  return _mm256_loadu_si256((__m256i *)addr);
}

template <>
inline float32x16 load<float32, 16>(const void *addr) {
  return _mm512_loadu_ps((float32 *)addr);
}

template <>
inline vec<int32, 16> load<int32, 16>(const void *addr) {
  return _mm512_loadu_si512(addr);
}

//*****************************************************************************

template <typename T, int dim>
inline vec<T, dim> load1(const void *addr);

template <>
inline float32x1 load1<float32, 1>(const void *addr) {
  return *(float32 *)addr;
}

template <>
inline float32x4 load1<float32, 4>(const void *addr) {
  return _mm_broadcast_ss((float32 *)addr);
}

template <>
inline float32x8 load1<float32, 8>(const void *addr) {
  return _mm256_broadcast_ss((float32 *)addr);
}

template <>
inline int32x1 load1<int32, 1>(const void *addr) {
  return *(int32 *)addr;
}

template <>
inline int32x4 load1<int32, 4>(const void *addr) {
  return union_cast<int32x4>(load1<float32, 4>(addr));
}

template <>
inline int32x8 load1<int32, 8>(const void *addr) {
  return union_cast<int32x8>(load1<float32, 8>(addr));
}

//*****************************************************************************
template <>
inline float32x1 gather<float32, 1>(const void *addr, int32x1 offsets) {
  // return _mm256_i32gather_ps((float32 *)addr, offsets, sizeof(float32));
  return *(float32 *)((uint8 *)addr + offsets.v * 4);
}

template <>
inline int32x1 gather<int32, 1>(const void *addr, int32x1 offsets) {
  // return _mm256_i32gather_ps((float32 *)addr, offsets, sizeof(float32));
  return *(int32 *)((uint8 *)addr + offsets.v * 4);
}

template <>
inline float32x4 gather<float32, 4>(const void *addr, int32x4 offsets) {
  // return _mm256_i32gather_ps((float32 *)addr, offsets, sizeof(float32));
  return _mm_i32gather_ps((float32 *)addr, offsets, 4);
}

template <>
inline int32x4 gather<int32, 4>(const void *addr, int32x4 offsets) {
  // return _mm256_i32gather_ps((float32 *)addr, offsets, sizeof(float32));
  return _mm_i32gather_epi32((int32 *)addr, offsets, 4);
}

template <>
inline float32x8 gather<float32, 8>(const void *addr, int32x8 offsets) {
  // return _mm256_i32gather_ps((float32 *)addr, offsets, sizeof(float32));
  return _mm256_i32gather_ps((float32 *)addr, offsets, 4);
}

template <>
inline int32x8 gather<int32, 8>(const void *addr, int32x8 offsets) {
  // return _mm256_i32gather_ps((float32 *)addr, offsets, sizeof(float32));
  return _mm256_i32gather_epi32((int32 *)addr, offsets, 4);
}

//*****************************************************************************

template <>
inline void store<float32, 1>(const float32x1 &v, const void *addr) {
  *(float32x1 *)(addr) = v;
}

template <>
inline void store<int32, 1>(const int32x1 &v, const void *addr) {
  *(int32x1 *)(addr) = v;
}

template <>
inline void store<float32, 4>(const float32x4 &v, const void *addr) {
  _mm_storeu_ps((float32 *)addr, v);
}

template <>
inline void store<int32, 4>(const int32x4 &v, const void *addr) {
  _mm_storeu_si128((__m128i *)addr, v);
}

template <>
inline void store<float32, 8>(const float32x8 &v, const void *addr) {
  _mm256_storeu_ps((float32 *)addr, v);
}

template <>
inline void store<int32, 8>(const int32x8 &v, const void *addr) {
  _mm256_storeu_si256((__m256i *)addr, v);
}

template <>
inline void store<float32, 16>(const float32x16 &v, const void *addr) {
  _mm512_storeu_ps((float32 *)addr, v);
}

template <>
inline void store<int32, 16>(const int32x16 &v, const void *addr) {
  _mm512_storeu_si512((__m512i *)addr, v);
}

//*****************************************************************************

template <>
inline void store<float32, 8>(const float32x8 &v, void *addr, int32x8 offsets) {
  // _mm256_i32scatter_ps(addr, offsets, v, sizeof(float32));
  for (int i = 0; i < 8; i++) {
    auto off = ((int *)&offsets)[i];
    ((float32 *)addr)[off] = v[i];
    // std::cout << off << "," << v[i] << std::endl;
  }
}

//*****************************************************************************

template <typename T, int dim>
inline vec<T, dim> floor(const vec<T, dim> &);

template <>
inline float32x1 floor<float32, 1>(const float32x1 &v) {
  return std::floor(v);
}

template <>
inline float32x4 floor<float32, 4>(const float32x4 &v) {
  return _mm_floor_ps(v);
}

template <>
inline float32x8 floor<float32, 8>(const float32x8 &v) {
  return _mm256_floor_ps(v);
}

//*****************************************************************************

template <typename G, typename T, int dim>
inline vec<G, dim> cast(const vec<T, dim> &);

template <>
inline int32x1 cast<int32, float32, 1>(const float32x1 &v) {
  return int32(floor(v));
}

template <>
inline float32x1 cast<float32, int32, 1>(const int32x1 &v) {
  return float32(v);
}

template <>
inline int32x4 cast<int32, float32, 4>(const float32x4 &v) {
  return _mm_cvtps_epi32(floor(v));
}

template <>
inline float32x4 cast<float32, int32, 4>(const int32x4 &v) {
  return _mm_cvtepi32_ps(v);
}

template <>
inline int32x8 cast<int32, float32, 8>(const float32x8 &v) {
  return _mm256_cvtps_epi32(floor(v));
}

template <>
inline float32x8 cast<float32, int32, 8>(const int32x8 &v) {
  return _mm256_cvtepi32_ps(v);
}

//*****************************************************************************

template <>
inline float32x1 set1<float32, 1>(float32 v) {
  return v;
}

template <>
inline int32x1 set1<int32, 1>(int32 v) {
  return v;
}

template <>
inline float32x4 set1<float32, 4>(float32 v) {
  return _mm_set1_ps(v);
}

template <>
inline int32x4 set1<int32, 4>(int32 v) {
  return _mm_set1_epi32(v);
}

template <>
inline float32x8 set1<float32, 8>(float32 v) {
  return _mm256_set1_ps(v);
}

template <>
inline int32x8 set1<int32, 8>(int32 v) {
  return _mm256_set1_epi32(v);
}

template <>
inline float32x16 set1<float32, 16>(float32 v) {
  return _mm512_set1_ps(v);
}

template <>
inline int32x16 set1<int32, 16>(int32 v) {
  return _mm512_set1_epi32(v);
}

//*****************************************************************************
inline float32x1 abs(float32x1 v) {
  return std::abs(v);
}

// https://github.com/MonoS/RGVS/blob/master/Repair.cpp
inline float32x8 abs(float32x8 v) {
  static __m256 Mask = _mm256_castsi256_ps(_mm256_set1_epi32(~0x80000000));
  return _mm256_and_ps(Mask, v);
}

template <int dim>
inline vec<float32, dim> sin(const vec<float32, dim> &v) {
  vec<float32, dim> ret;
  for (int i = 0; i < dim; i++) {
    ret[i] = std::sin(v[i]);
  }
  return ret;
}

template <int dim>
inline vec<float32, dim> cos(const vec<float32, dim> &v) {
  vec<float32, dim> ret;
  for (int i = 0; i < dim; i++) {
    ret[i] = std::cos(v[i]);
  }
  return ret;
}

//*****************************************************************************

template <typename T, int dim>
inline vec<T, dim> min(vec<T, dim>, vec<T, dim>);

template <>
inline int32x8 min<int32, 8>(int32x8 a, int32x8 b) {
  return _mm256_min_epi32(a, b);
}

template <>
inline float32x8 min<float32, 8>(float32x8 a, float32x8 b) {
  return _mm256_min_ps(a, b);
}

template <>
inline float32x4 min<float32, 4>(float32x4 a, float32x4 b) {
  return _mm_min_ps(a, b);
}

template <>
inline float32x1 min<float32, 1>(float32x1 a, float32x1 b) {
  return std::min(a, b);
}

template <typename T, int dim>
inline vec<T, dim> max(vec<T, dim>, vec<T, dim>);

template <>
inline int32x8 max<int32, 8>(int32x8 a, int32x8 b) {
  return _mm256_max_epi32(a, b);
}

template <>
inline float32x8 max<float32, 8>(float32x8 a, float32x8 b) {
  return _mm256_max_ps(a, b);
}

template <>
inline float32x1 max<float32, 1>(float32x1 a, float32x1 b) {
  return std::max(a, b);
}

//*****************************************************************************
inline int32x1 cmp_ne(float32x1 a, float32x1 b) {
  return int32(a.v != b.v) * -1;
}

inline int32x1 cmp_ne(int32x1 a, int32x1 b) {
  return int32(a.v != b.v) * -1;
}

inline int32x4 cmp_ne(float32x4 a, float32x4 b) {
  auto ret = _mm_cmp_ps(a, b, _CMP_NEQ_UQ);
  return union_cast<int32x4>(ret);
}

inline int32x4 cmp_ne(int32x4 a, int32x4 b) {
  auto ret = _mm_cmp_ps(union_cast<float32x4>(a), union_cast<float32x4>(b),
                        _CMP_NEQ_UQ);
  return union_cast<int32x4>(ret);
}

inline int32x8 cmp_ne(float32x8 a, float32x8 b) {
  auto ret = _mm256_cmp_ps(a, b, _CMP_NEQ_UQ);
  return union_cast<int32x8>(ret);
}

inline int32x8 cmp_ne(int32x8 a, int32x8 b) {
  auto ret = _mm256_cmp_ps(union_cast<float32x8>(a), union_cast<float32x8>(b),
                           _CMP_NEQ_UQ);
  return union_cast<int32x8>(ret);
}

inline int32x1 cmp_eq(float32x1 a, float32x1 b) {
  return int32(a.v == b.v) * -1;
}

inline int32x1 cmp_eq(int32x1 a, int32x1 b) {
  return int32(a.v == b.v) * -1;
}

inline int32x4 cmp_eq(float32x4 a, float32x4 b) {
  auto ret = _mm_cmp_ps(a, b, _CMP_EQ_UQ);
  return union_cast<int32x4>(ret);
}

inline int32x4 cmp_eq(int32x4 a, int32x4 b) {
  auto ret = _mm_cmp_ps(union_cast<float32x4>(a), union_cast<float32x4>(b),
                        _CMP_EQ_UQ);
  return union_cast<int32x4>(ret);
}

inline int32x8 cmp_eq(float32x8 a, float32x8 b) {
  auto ret = _mm256_cmp_ps(a, b, _CMP_EQ_UQ);
  return union_cast<int32x8>(ret);
}

inline int32x8 cmp_eq(int32x8 a, int32x8 b) {
  auto ret = _mm256_cmp_ps(union_cast<float32x8>(a), union_cast<float32x8>(b),
                           _CMP_EQ_UQ);
  return union_cast<int32x8>(ret);
}

inline int32x1 cmp_lt(float32x1 a, float32x1 b) {
  return int(a < b) * -1;
}

inline int32x1 cmp_lt(int32x1 a, int32x1 b) {
  return int(a < b) * -1;
}

inline int32x4 cmp_lt(float32x4 a, float32x4 b) {
  auto ret = _mm_cmp_ps(a, b, _CMP_LT_OQ);
  return union_cast<int32x4>(ret);
}

inline int32x4 cmp_lt(int32x4 a, int32x4 b) {
  auto ret = _mm_cmpgt_epi32(b, a);
  return ret;
}

inline int32x8 cmp_lt(float32x8 a, float32x8 b) {
  auto ret = _mm256_cmp_ps(a, b, _CMP_LT_OQ);
  return union_cast<int32x8>(ret);
}

inline int32x8 cmp_lt(int32x8 a, int32x8 b) {
  auto ret = _mm256_cmpgt_epi32(b, a);
  return ret;
}

inline int32x1 cmp_gt(float32x1 b, float32x1 a) {
  return int(a < b) * -1;
}

inline int32x1 cmp_gt(int32x1 b, int32x1 a) {
  return int(a < b) * -1;
}

inline int32x4 cmp_gt(float32x4 b, float32x4 a) {
  auto ret = _mm_cmp_ps(a, b, _CMP_LT_OQ);
  return union_cast<int32x4>(ret);
}

inline int32x4 cmp_gt(int32x4 b, int32x4 a) {
  auto ret = _mm_cmpgt_epi32(b, a);
  return ret;
}

inline int32x8 cmp_gt(float32x8 b, float32x8 a) {
  auto ret = _mm256_cmp_ps(a, b, _CMP_LT_OQ);
  return union_cast<int32x8>(ret);
}

inline int32x8 cmp_gt(int32x8 b, int32x8 a) {
  auto ret = _mm256_cmpgt_epi32(b, a);
  return ret;
}

//*****************************************************************************

inline float32x1 select(int32x1 mask, float32x1 true_val, float32x1 false_val) {
  return mask ? true_val : false_val;
}

inline int32x1 select(int32x1 mask, int32x1 true_val, int32x1 false_val) {
  return mask ? true_val : false_val;
}

inline float32x4 select(int32x4 mask, float32x4 true_val, float32x4 false_val) {
  return _mm_blendv_ps(false_val, true_val, union_cast<float32x4>(mask));
}

inline int32x4 select(int32x4 mask, int32x4 true_val, int32x4 false_val) {
  auto ret = _mm_blendv_ps(union_cast<float32x4>(false_val),
                           union_cast<float32x4>(true_val),
                           union_cast<float32x4>(mask));
  return union_cast<int32x4>(ret);
}

inline float32x8 select(int32x8 mask, float32x8 true_val, float32x8 false_val) {
  return _mm256_blendv_ps(false_val, true_val, union_cast<float32x8>(mask));
}

inline int32x8 select(int32x8 mask, int32x8 true_val, int32x8 false_val) {
  auto ret = _mm256_blendv_ps(union_cast<float32x8>(false_val),
                              union_cast<float32x8>(true_val),
                              union_cast<float32x8>(mask));
  return union_cast<int32x8>(ret);
}

//*****************************************************************************
inline bool any(int32x1 v) {
  return v;
}

inline bool any(int32x4 v) {
  return _mm_movemask_ps(union_cast<__m128>(v));
}

inline bool any(int32x8 v) {
  return _mm256_movemask_ps(union_cast<__m256>(v));
}

//*****************************************************************************

template <int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7>
inline int32x8 shuffle8x32(int32x8 a) {
  return _mm256_permutevar8x32_epi32(
      a, _mm256_set_epi32(i7, i6, i5, i4, i3, i2, i1, i0));
};

template <int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7>
inline float32x8 shuffle8x32(float32x8 a) {
  return _mm256_permutevar8x32_ps(
      a, _mm256_set_epi32(i7, i6, i5, i4, i3, i2, i1, i0));
};

template <int imm>
inline float32x4 blend(float32x4 a, float32x4 b) {
  return _mm_blend_ps(a, b, imm);
}

template <int imm>
inline float32x8 blend(float32x8 a, float32x8 b) {
  return _mm256_blend_ps(a, b, imm);
}

template <int imm>
inline int32x8 blend(int32x8 a, int32x8 b) {
  return _mm256_blend_epi32(a, b, imm);
}

inline int32x1 bit_not(int32x1 a) {
  return int(-1) ^ a;
}

inline int32x8 bit_not(int32x8 a) {
  return _mm256_xor_si256(a, _mm256_set1_epi64x(-1LL));
}

#define DEFINE_BINARY_OP(T, OP, INST) \
  inline T OP(T a, T b) {             \
    return INST(a, b);                \
  }

DEFINE_BINARY_OP(float32x4, add, _mm_add_ps);
DEFINE_BINARY_OP(float32x4, sub, _mm_sub_ps);
DEFINE_BINARY_OP(float32x4, mul, _mm_mul_ps);
DEFINE_BINARY_OP(float32x4, div, _mm_div_ps);
DEFINE_BINARY_OP(float32x4, min, _mm_min_ps);
DEFINE_BINARY_OP(float32x4, max, _mm_max_ps);

DEFINE_BINARY_OP(int32x4, add, _mm_add_epi32);
DEFINE_BINARY_OP(int32x4, sub, _mm_sub_epi32);
DEFINE_BINARY_OP(int32x4, mul, _mm_mullo_epi32);
DEFINE_BINARY_OP(int32x4, min, _mm_min_epi32);
DEFINE_BINARY_OP(int32x4, max, _mm_max_epi32);
DEFINE_BINARY_OP(int32x4, bit_and, _mm_and_si128);
DEFINE_BINARY_OP(int32x4, bit_or, _mm_or_si128);

DEFINE_BINARY_OP(float32x8, add, _mm256_add_ps);
DEFINE_BINARY_OP(float32x8, sub, _mm256_sub_ps);
DEFINE_BINARY_OP(float32x8, mul, _mm256_mul_ps);
DEFINE_BINARY_OP(float32x8, div, _mm256_div_ps);
DEFINE_BINARY_OP(float32x8, min, _mm256_min_ps);
DEFINE_BINARY_OP(float32x8, max, _mm256_max_ps);

DEFINE_BINARY_OP(float32x16, add, _mm512_add_ps);
DEFINE_BINARY_OP(float32x16, sub, _mm512_sub_ps);
DEFINE_BINARY_OP(float32x16, mul, _mm512_mul_ps);
DEFINE_BINARY_OP(float32x16, div, _mm512_div_ps);
DEFINE_BINARY_OP(float32x16, min, _mm512_min_ps);
DEFINE_BINARY_OP(float32x16, max, _mm512_max_ps);

DEFINE_BINARY_OP(int32x8, add, _mm256_add_epi32);
DEFINE_BINARY_OP(int32x8, sub, _mm256_sub_epi32);
DEFINE_BINARY_OP(int32x8, mul, _mm256_mullo_epi32);
DEFINE_BINARY_OP(int32x8, min, _mm256_min_epi32);
DEFINE_BINARY_OP(int32x8, max, _mm256_max_epi32);
DEFINE_BINARY_OP(int32x8, bit_and, _mm256_and_si256);
DEFINE_BINARY_OP(int32x8, bit_or, _mm256_or_si256);

DEFINE_BINARY_OP(int32x16, add, _mm512_add_epi32);
DEFINE_BINARY_OP(int32x16, sub, _mm512_sub_epi32);
DEFINE_BINARY_OP(int32x16, mul, _mm512_mullo_epi32);
DEFINE_BINARY_OP(int32x16, min, _mm512_min_epi32);
DEFINE_BINARY_OP(int32x16, max, _mm512_max_epi32);
DEFINE_BINARY_OP(int32x16, bit_and, _mm512_and_si512);
DEFINE_BINARY_OP(int32x16, bit_or, _mm512_or_si512);

#define DEFINE_BINARY_OP_MID(T, OP, INST) \
  inline T OP(T a, T b) {                 \
    return a INST b;                      \
  }

DEFINE_BINARY_OP_MID(float32x1, add, +);
DEFINE_BINARY_OP_MID(float32x1, sub, -);
DEFINE_BINARY_OP_MID(float32x1, mul, *);
DEFINE_BINARY_OP_MID(float32x1, div, /);
DEFINE_BINARY_OP_MID(int32x1, add, +);
DEFINE_BINARY_OP_MID(int32x1, sub, -);
DEFINE_BINARY_OP_MID(int32x1, mul, *);
DEFINE_BINARY_OP_MID(int32x1, div, /);
DEFINE_BINARY_OP_MID(int32x1, mod, %);
DEFINE_BINARY_OP_MID(int32x1, bit_and, &);
DEFINE_BINARY_OP_MID(int32x1, bit_or, |);

inline int32x8 shr(int32x8 a, int b) {
  return _mm256_srli_epi32(a, b);
}

inline int32x8 shl(int32x8 a, int b) {
  return _mm256_slli_epi32(a, b);
}

inline int32x8 bit_and(int32x8 a, int b) {
  int32x8 B = _mm256_set1_epi32(b);
  int32x8 v = _mm256_and_si256(a, B);
  return v;
}

inline float32x8 sqrt(float32x8 v) {
  return _mm256_sqrt_ps(v);
}

inline float32x4 sqrt(float32x4 v) {
  return _mm_sqrt_ps(v);
}

inline float32x1 sqrt(float32x1 v) {
  return std::sqrt(v);
}

inline float32x8 inv(float32x8 v) {
  return _mm256_rcp_ps(v);
}

inline float32x1 inv(float32x1 v) {
  return 1.0f / v;
}

inline float32x1 neg(float32x1 v) {
  // TODO: optimize
  return -v;
}

inline float32x4 neg(float32x4 v) {
  // TODO: optimize
  return sub(float32x4(0), v);
}

inline float32x8 neg(float32x8 v) {
  // TODO: optimize
  return sub(float32x8(0), v);
}

template <int dim>
inline vec<int32, dim> div(vec<int32, dim> a, vec<int32, dim> b) {
  vec<int32, dim> ret;
  for (int i = 0; i < dim; i++) {
    ret[i] = a[i] / b[i];
  }
  return ret;
};

template <int dim>
inline vec<int32, dim> mod(vec<int32, dim> a, vec<int32, dim> b) {
  // static_assert(std::is_integral<T>::value, "");
  // return _mm256_and_si256(a, _mm256_set1_epi32(511));
  return sub(a, mul(div(a, b), b));
}

template <int dim>
inline vec<float32, dim> mod(vec<float32, dim> a, vec<float32, dim> b) {
  return sub(a, mul(floor(div(a, b)), b));
};

#endif  // Intrinsics wrapper

#if defined(TC_GPU)

using float32x1 = float32;
using int32x1 = int32;
using float64x1 = float64;

#define DEFINE_CUDA_OP(name, op)                 \
  template <typename T>                          \
  __device__ auto name(const T &a, const T &b) { \
    return a op b;                               \
  }

DEFINE_CUDA_OP(add, +)
DEFINE_CUDA_OP(sub, -)
DEFINE_CUDA_OP(mul, *)
DEFINE_CUDA_OP(div, /)
DEFINE_CUDA_OP(mod, %)
DEFINE_CUDA_OP(bit_and, &)
DEFINE_CUDA_OP(bit_or, |)

DEFINE_CUDA_OP(cmp_le, <=)
DEFINE_CUDA_OP(cmp_eq, ==)

#define DEFINE_CUDA_UNARY_OP(name, op) \
  template <typename T>                \
  __device__ auto name(const T &a) {   \
    return op a;                       \
  }

DEFINE_CUDA_UNARY_OP(bit_not, ~)

template <typename T, typename G>
__device__ auto select(const G &flag, const T &a, const T &b) {
  return flag ? a : b;
}

#endif

// *****************************************************************************
// these structures are used for maintaining metadata and sparsity.
// Their look_up function takes a merged index, but they don't know where do the
// bits come from.

#if defined(TLANG_KERNEL)
#define TC_EXPORT
#if defined(TC_GPU)
#define TC_DEVICE __device__ __host__
#define TLANG_ACCESSOR __device__ TC_FORCE_INLINE
#else
#define TC_DEVICE
#define TLANG_ACCESSOR TC_FORCE_INLINE
#endif
#else
#define TLANG_ACCESSOR
#define TC_EXPORT extern "C"
#define TC_DEVICE
#endif

template <typename child_type>
struct layout_root {
  child_type children;
  TC_DEVICE TC_FORCE_INLINE child_type *look_up(
      int i) {  // i is flattened index
    return &children;
  }

  TC_DEVICE TC_FORCE_INLINE int get_n() const {
    return 1;
  }

  static constexpr bool has_null = false;
};

template <typename child_type, int n_>
struct fixed {
  static constexpr int n = n_;
  child_type children[n];
  TC_DEVICE TC_FORCE_INLINE child_type *look_up(
      int i) {  // i is flattened index
    return &children[i];
  }

  TC_DEVICE TC_FORCE_INLINE int get_n() const {
    return n;
  }

  static constexpr bool has_null = false;
};

template <typename _child_type>
struct hashed {
  using child_type = _child_type;
  std::unordered_map<int, child_type> data;
  std::mutex mut;
  TC_DEVICE TC_FORCE_INLINE child_type *look_up(
      int i) {  // i is flattened index
#if defined(TLANG_HOST)
    if (data.find(i) == data.end()) {
      std::memset(&data[i], 0, sizeof(data[i]));
    }
#else
    if (data.find(i) == data.end()) {
      return nullptr;
    }
#endif
    return &data[i];
  }

  TC_DEVICE TC_FORCE_INLINE void touch(int i) {
    TC_ASSERT(false);
    // printf("p=%p\n", &n);
    // printf("n=%d, i=%d\n", (int)n, i);
  }

  TC_DEVICE TC_FORCE_INLINE int get_n() const {
    return data.size();
  }

  static constexpr bool has_null = true;
};

template <typename _child_type>
struct pointer {
  using child_type = _child_type;
  child_type *data;
  // std::mutex mut;
  TC_DEVICE TC_FORCE_INLINE child_type *look_up(
      int i) {  // i is flattened index
#if defined(TLANG_HOST)
    touch(i);
#endif
    return data;
  }

  TC_DEVICE TC_FORCE_INLINE void touch(int i) {
    // std::lock_guard<std::mutex> _(mut);
    if (data == nullptr) {
      data = new child_type;
      std::memset(data, 0, sizeof(child_type));
    }
  }

  TC_DEVICE TC_FORCE_INLINE int get_n() const {
    return 1;
  }

  static constexpr bool has_null = true;
};

template <typename _child_type, int max_n_>
struct dynamic {
  static constexpr int max_n = max_n_;
  using child_type = _child_type;
  child_type data[max_n];
  std::atomic<int> n;

  TC_DEVICE dynamic() : n(0) {
  }

  TC_DEVICE TC_FORCE_INLINE child_type *look_up(
      int i) {  // i is flattened index
#if defined(TLANG_HOST)
    n.store(std::max(n.load(), i + 1));
#endif
    return &data[i];
  }

  TC_DEVICE TC_FORCE_INLINE void touch(child_type t) {
    data[n++] = t;
    // printf("p=%p\n", &n);
    // printf("n=%d, i=%d\n", (int)n, i);
  }

  TC_DEVICE TC_FORCE_INLINE int get_n() const {
    return n.load();
  }

  static constexpr bool has_null = false;
};
// *****************************************************************************

template <int max_n_>
struct indirect {
  static constexpr int max_n = max_n_;
  int data[max_n];
  std::atomic<int> n;

  TC_DEVICE indirect() : n(0) {
  }

  TC_DEVICE TC_FORCE_INLINE int get_n() {
    return n;
  }

  TC_DEVICE TC_FORCE_INLINE int *look_up(int i) {  // i is flattened index
#if defined(TLANG_HOST)
    n.store(std::max(n.load(), i + 1));
#endif
    return &data[i];
  }

  TC_DEVICE TC_FORCE_INLINE void touch(int i) {
    data[n++] = i;
    // printf("p=%p\n", &n);
    // printf("n=%d, i=%d\n", (int)n, i);
  }

  TC_DEVICE TC_FORCE_INLINE void clear() {
    n.store(0);
  }

  static constexpr bool has_null = false;
};
// *****************************************************************************

}  // namespace Tlang

}  // namespace taichi
