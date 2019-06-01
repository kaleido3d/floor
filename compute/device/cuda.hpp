/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License only.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __FLOOR_COMPUTE_DEVICE_CUDA_HPP__
#define __FLOOR_COMPUTE_DEVICE_CUDA_HPP__

#if defined(FLOOR_COMPUTE_CUDA)

#include <floor/math/constants.hpp>
#include <floor/compute/device/soft_printf.hpp>

// misc (not directly defined by cuda?)
#define cuda_sm_id __builtin_ptx_read_smid()
#define cuda_sm_dim __builtin_ptx_read_nsmid()
#define cuda_grid_id __builtin_ptx_read_gridid()

#define cuda_lanemask_eq __builtin_ptx_read_lanemask_eq()
#define cuda_lanemask_le __builtin_ptx_read_lanemask_le()
#define cuda_lanemask_lt __builtin_ptx_read_lanemask_lt()
#define cuda_lanemask_ge __builtin_ptx_read_lanemask_ge()
#define cuda_lanemask_gt __builtin_ptx_read_lanemask_gt()

#define cuda_clock __builtin_ptx_read_clock()
#define cuda_clock64 __builtin_ptx_read_clock64()

// we always have permute instructions
#define FLOOR_COMPUTE_INFO_HAS_PERMUTE 1

// we always have reverse bits instructions
#define FLOOR_COMPUTE_INFO_HAS_REVERSE_BITS_32 1
#define FLOOR_COMPUTE_INFO_HAS_REVERSE_BITS_64 1

// sm_32+ has funnel shift instructions
#if FLOOR_COMPUTE_INFO_CUDA_SM >= 32
#define FLOOR_COMPUTE_INFO_HAS_FUNNEL_SHIFT 1
#else
#define FLOOR_COMPUTE_INFO_HAS_FUNNEL_SHIFT 0
#endif

// sm_30+ with ptx 6.0+ has a "find nth set" instruction
#if FLOOR_COMPUTE_INFO_CUDA_SM >= 30 && FLOOR_COMPUTE_INFO_CUDA_PTX >= 60
#define FLOOR_COMPUTE_INFO_HAS_FIND_NTH_SET 1
#else
#define FLOOR_COMPUTE_INFO_HAS_FIND_NTH_SET 0
#endif

// shfl.sync was introduced with PTX 6.0 and is required for Volta
// -> for backwards compat, use the non-sync version when targeting PTX < 6.0
#if FLOOR_COMPUTE_INFO_CUDA_PTX >= 60
#define FLOOR_CUDA_SHFL "shfl.sync"
#define FLOOR_CUDA_SHFL_MASK ", 0xFFFFFFFF"
#else
#define FLOOR_CUDA_SHFL "shfl"
#define FLOOR_CUDA_SHFL_MASK
#endif

namespace std {
	// float math functions
	const_func floor_inline_always float sqrt(float a) { return __nvvm_sqrt_rz_ftz_f(a); }
	const_func floor_inline_always float rsqrt(float a) { return __nvvm_rsqrt_approx_ftz_f(a); }
	const_func floor_inline_always float fmod(float x, float y) { return x - y * __nvvm_trunc_ftz_f(x / y); }
	const_func floor_inline_always float fabs(float a) { return __nvvm_fabs_ftz_f(a); }
	const_func floor_inline_always float abs(float a) { return fabs(a); }
	const_func floor_inline_always float floor(float a) { return __nvvm_floor_ftz_f(a); }
	const_func floor_inline_always float ceil(float a) { return __nvvm_ceil_ftz_f(a); }
	const_func floor_inline_always float round(float a) { return __nvvm_round_ftz_f(a); }
	const_func floor_inline_always float trunc(float a) { return __nvvm_trunc_ftz_f(a); }
	const_func floor_inline_always float rint(float a) { return __nvvm_trunc_ftz_f(a); }
	const_func floor_inline_always float fmin(float a, float b) { return __nvvm_fmin_ftz_f(a, b); }
	const_func floor_inline_always float fmax(float a, float b) { return __nvvm_fmax_ftz_f(a, b); }
	
	const_func floor_inline_always float sin(float a) { return __nvvm_sin_approx_ftz_f(a); }
	const_func floor_inline_always float cos(float a) { return __nvvm_cos_approx_ftz_f(a); }
	const_func floor_inline_always float tan(float a) { return __nvvm_sin_approx_ftz_f(a) / __nvvm_cos_approx_ftz_f(a); }
	
	const_func floor_inline_always float fma(float a, float b, float c) { return __nvvm_fma_rz_ftz_f(a, b, c); }
	const_func floor_inline_always float pow(float a, float b) { return __nvvm_ex2_approx_ftz_f(b * __nvvm_lg2_approx_ftz_f(a)); }
	const_func floor_inline_always float exp(float a) { return __nvvm_ex2_approx_ftz_f(a * const_math::_1_DIV_LN_2<float>); } // 2^(x / ln(2))
	const_func floor_inline_always float exp2(float a) { return __nvvm_ex2_approx_ftz_f(a); }
	const_func floor_inline_always float log(float a) { return __nvvm_lg2_approx_ftz_f(a) * const_math::_1_DIV_LD_E<float>; } // log_e(x) = log_2(x) / log_2(e)
	const_func floor_inline_always float log2(float a) { return __nvvm_lg2_approx_ftz_f(a); }
	
	const_func floor_inline_always float copysign(float a, float b) {
		float ret;
		// NOTE: ptx has the a and b parameter reversed (in comparison to std c++ / llvm / opencl)
		asm("copysign.f32 %0, %1, %2;" : "=f"(ret) : "f"(b), "f"(a));
		return ret;
	}
	
	// double math functions
	const_func floor_inline_always double sqrt(double a) { return __nvvm_sqrt_rz_d(a); }
	const_func floor_inline_always double rsqrt(double a) { return __nvvm_rsqrt_approx_d(a); }
	const_func floor_inline_always double fmod(double x, double y) { return x - y * __nvvm_trunc_d(x / y); }
	const_func floor_inline_always double fabs(double a) { return __nvvm_fabs_d(a); }
	const_func floor_inline_always double abs(double a) { return fabs(a); }
	const_func floor_inline_always double floor(double a) { return __nvvm_floor_d(a); }
	const_func floor_inline_always double ceil(double a) { return __nvvm_ceil_d(a); }
	const_func floor_inline_always double round(double a) { return __nvvm_round_d(a); }
	const_func floor_inline_always double trunc(double a) { return __nvvm_trunc_d(a); }
	const_func floor_inline_always double rint(double a) { return __nvvm_trunc_d(a); }
	const_func floor_inline_always double fmin(double a, double b) { return __nvvm_fmin_d(a, b); }
	const_func floor_inline_always double fmax(double a, double b) { return __nvvm_fmax_d(a, b); }
	
	const_func floor_inline_always double sin(double a) { return double(__nvvm_sin_approx_ftz_f(float(a))); }
	const_func floor_inline_always double cos(double a) { return double(__nvvm_cos_approx_ftz_f(float(a))); }
	const_func floor_inline_always double tan(double a) { return double(__nvvm_sin_approx_ftz_f(float(a))) / double(__nvvm_cos_approx_ftz_f(float(a))); }
	
	const_func floor_inline_always double fma(double a, double b, double c) { return __nvvm_fma_rz_d(a, b, c); }
	// NOTE: even though there are intrinsics for this, there are no double/f64 versions supported in h/w
	const_func floor_inline_always double pow(double a, double b) { return double(__nvvm_ex2_approx_ftz_f(float(b) * __nvvm_lg2_approx_ftz_f(float(a)))); }
	const_func floor_inline_always double exp(double a) { return double(__nvvm_ex2_approx_ftz_f(float(a) * const_math::_1_DIV_LN_2<float>)); } // 2^(x / ln(2))
	const_func floor_inline_always double exp2(double a) { return (double)__nvvm_ex2_approx_ftz_f(float(a)); }
	const_func floor_inline_always double log(double a) { return double(__nvvm_lg2_approx_ftz_f(float(a))) * const_math::_1_DIV_LD_E<float>; } // log_e(x) = log_2(x) / log_2(e)
	const_func floor_inline_always double log2(double a) { return (double)__nvvm_lg2_approx_ftz_f(float(a)); }
	
	const_func floor_inline_always double copysign(double a, double b) {
		double ret;
		asm("copysign.f64 %0, %1, %2;" : "=d"(ret) : "d"(b), "d"(a));
		return ret;
	}
	
	// int math functions
	const_func floor_inline_always int8_t abs(int8_t a) {
		return a < int8_t(0) ? -a : a;
	}
	const_func floor_inline_always int16_t abs(int16_t a) {
		int16_t ret;
		asm("abs.s16 %0, %1;" : "=h"(ret) : "h"(a));
		return ret;
	}
	const_func floor_inline_always int32_t abs(int32_t a) {
		int32_t ret;
		asm("abs.s32 %0, %1;" : "=r"(ret) : "r"(a));
		return ret;
	}
	const_func floor_inline_always int64_t abs(int64_t a) {
		int64_t ret;
		asm("abs.s64 %0, %1;" : "=l"(ret) : "l"(a));
		return ret;
	}
	const_func floor_inline_always uint8_t abs(uint8_t a) { return a; }
	const_func floor_inline_always uint16_t abs(uint16_t a) { return a; }
	const_func floor_inline_always uint32_t abs(uint32_t a) { return a; }
	const_func floor_inline_always uint64_t abs(uint64_t a) { return a; }
	
	const_func floor_inline_always int16_t floor_rt_min(int16_t a, int16_t b) {
		int16_t ret;
		asm("min.s16 %0, %1, %2;" : "=h"(ret) : "h"(a), "h"(b));
		return ret;
	}
	const_func floor_inline_always uint16_t floor_rt_min(uint16_t a, uint16_t b) {
		uint16_t ret;
		asm("min.u16 %0, %1, %2;" : "=h"(ret) : "h"(a), "h"(b));
		return ret;
	}
	const_func floor_inline_always int16_t floor_rt_max(int16_t a, int16_t b) {
		int16_t ret;
		asm("max.s16 %0, %1, %2;" : "=h"(ret) : "h"(a), "h"(b));
		return ret;
	}
	const_func floor_inline_always uint16_t floor_rt_max(uint16_t a, uint16_t b) {
		uint16_t ret;
		asm("max.u16 %0, %1, %2;" : "=h"(ret) : "h"(a), "h"(b));
		return ret;
	}
	const_func floor_inline_always int8_t floor_rt_min(int8_t a, int8_t b) {
		return (int8_t)floor_rt_min(int16_t(a), int16_t(b));
	}
	const_func floor_inline_always uint8_t floor_rt_min(uint8_t a, uint8_t b) {
		return (uint8_t)floor_rt_min(uint16_t(a), uint16_t(b));
	}
	const_func floor_inline_always int8_t floor_rt_max(int8_t a, int8_t b) {
		return (int8_t)floor_rt_max(int16_t(a), int16_t(b));
	}
	const_func floor_inline_always uint8_t floor_rt_max(uint8_t a, uint8_t b) {
		return (uint8_t)floor_rt_max(uint16_t(a), uint16_t(b));
	}
#if FLOOR_TOOLCHAIN_VERSION <= 40000
	const_func floor_inline_always int32_t floor_rt_min(int32_t a, int32_t b) { return __nvvm_min_i(a, b); }
	const_func floor_inline_always uint32_t floor_rt_min(uint32_t a, uint32_t b) { return __nvvm_min_ui(a, b); }
	const_func floor_inline_always int64_t floor_rt_min(int64_t a, int64_t b) { return __nvvm_min_ll(a, b); }
	const_func floor_inline_always uint64_t floor_rt_min(uint64_t a, uint64_t b) { return __nvvm_min_ull(a, b); }
	const_func floor_inline_always float floor_rt_min(float a, float b) { return __nvvm_fmin_ftz_f(a, b); }
	const_func floor_inline_always double floor_rt_min(double a, double b) { return __nvvm_fmin_d(a, b); }
	const_func floor_inline_always int32_t floor_rt_max(int32_t a, int32_t b) { return __nvvm_max_i(a, b); }
	const_func floor_inline_always uint32_t floor_rt_max(uint32_t a, uint32_t b) { return __nvvm_max_ui(a, b); }
	const_func floor_inline_always int64_t floor_rt_max(int64_t a, int64_t b) { return __nvvm_max_ll(a, b); }
	const_func floor_inline_always uint64_t floor_rt_max(uint64_t a, uint64_t b) { return __nvvm_max_ull(a, b); }
	const_func floor_inline_always float floor_rt_max(float a, float b) { return __nvvm_fmax_ftz_f(a, b); }
	const_func floor_inline_always double floor_rt_max(double a, double b) { return __nvvm_fmax_d(a, b); }
#else
	// NOTE: these get optimized to min/max.s/u(16|32|64)
	const_func floor_inline_always constexpr int32_t floor_rt_min(int32_t a, int32_t b) { return (a < b ? a : b); }
	const_func floor_inline_always constexpr uint32_t floor_rt_min(uint32_t a, uint32_t b) { return (a < b ? a : b); }
	const_func floor_inline_always constexpr int64_t floor_rt_min(int64_t a, int64_t b) { return (a < b ? a : b); }
	const_func floor_inline_always constexpr uint64_t floor_rt_min(uint64_t a, uint64_t b) { return (a < b ? a : b); }
	const_func floor_inline_always constexpr float floor_rt_min(float a, float b) { return (a < b ? a : b); }
	const_func floor_inline_always constexpr double floor_rt_min(double a, double b) { return (a < b ? a : b); }
	const_func floor_inline_always constexpr int32_t floor_rt_max(int32_t a, int32_t b) { return (a > b ? a : b); }
	const_func floor_inline_always constexpr uint32_t floor_rt_max(uint32_t a, uint32_t b) { return (a > b ? a : b); }
	const_func floor_inline_always constexpr int64_t floor_rt_max(int64_t a, int64_t b) { return (a > b ? a : b); }
	const_func floor_inline_always constexpr uint64_t floor_rt_max(uint64_t a, uint64_t b) { return (a > b ? a : b); }
	const_func floor_inline_always constexpr float floor_rt_max(float a, float b) { return (a > b ? a : b); }
	const_func floor_inline_always constexpr double floor_rt_max(double a, double b) { return (a > b ? a : b); }
#endif
	
	const_func floor_inline_always size_t floor_rt_min(size_t a, size_t b) {
		return (size_t)floor_rt_min(uint64_t(a), uint64_t(b));
	}
	const_func floor_inline_always size_t floor_rt_max(size_t a, size_t b) {
		return (size_t)floor_rt_max(uint64_t(a), uint64_t(b));
	}
	
	const_func floor_inline_always uint32_t floor_rt_permute(const uint32_t low, const uint32_t high, const uint32_t select) {
		return __nvvm_prmt(low, high, select);
	}
	
#if FLOOR_COMPUTE_INFO_HAS_FUNNEL_SHIFT == 1
	const_func floor_inline_always uint32_t floor_rt_funnel_shift_left(const uint32_t low, const uint32_t high, const uint32_t shift) {
		uint32_t ret;
		asm("shf.l.wrap.b32 %0, %1, %2, %3;" : "=r"(ret) : "r"(low), "r"(high), "r"(shift));
		return ret;
	}
	
	const_func floor_inline_always uint32_t floor_rt_funnel_shift_right(const uint32_t low, const uint32_t high, const uint32_t shift) {
		uint32_t ret;
		asm("shf.r.wrap.b32 %0, %1, %2, %3;" : "=r"(ret) : "r"(low), "r"(high), "r"(shift));
		return ret;
	}
	
	const_func floor_inline_always uint32_t floor_rt_funnel_shift_clamp_left(const uint32_t low, const uint32_t high, const uint32_t shift) {
		uint32_t ret;
		asm("shf.l.clamp.b32 %0, %1, %2, %3;" : "=r"(ret) : "r"(low), "r"(high), "r"(shift));
		return ret;
	}
	
	const_func floor_inline_always uint32_t floor_rt_funnel_shift_clamp_right(const uint32_t low, const uint32_t high, const uint32_t shift) {
		uint32_t ret;
		asm("shf.r.clamp.b32 %0, %1, %2, %3;" : "=r"(ret) : "r"(low), "r"(high), "r"(shift));
		return ret;
	}
#endif
	
#if FLOOR_COMPUTE_INFO_CUDA_SM >= 30 && FLOOR_COMPUTE_INFO_CUDA_PTX >= 60
	const_func floor_inline_always uint32_t floor_rt_find_nth_set(const uint32_t value, const uint32_t base, const int32_t offset) {
		uint32_t ret = 0;
		asm("fns.b32 %0, %1, %2, %3;" : "=r"(ret) : "r"(value), "r"(base), "r"(offset));
		return ret;
	}
#endif
	
#if FLOOR_COMPUTE_INFO_CUDA_SM >= 30
	template <uint32_t width, typename any_type, typename = enable_if_t<sizeof(any_type) == 4>>
	volatile floor_inline_always any_type sub_group_shuffle_index(const any_type lane_var, const uint32_t src_lane_idx) {
		constexpr const auto mask = ((device_info::simd_width() - width) << 8u) | 0x1Fu;
		any_type ret;
		if constexpr(is_floating_point_v<any_type>) {
			asm volatile(FLOOR_CUDA_SHFL ".idx.b32 %0, %1, %2, %3" FLOOR_CUDA_SHFL_MASK ";"
						 : "=f"(ret) : "f"(lane_var), "r"(src_lane_idx), "i"(mask));
		}
		else {
			asm volatile(FLOOR_CUDA_SHFL ".idx.b32 %0, %1, %2, %3" FLOOR_CUDA_SHFL_MASK ";"
						  : "=r"(ret) : "r"(lane_var), "r"(src_lane_idx), "i"(mask));
		}
		return ret;
	}
#endif
	
	// asin/acos/atan s/w computation
	template <typename fp_type, typename = enable_if_t<ext::is_floating_point_v<fp_type>>>
	const_func floor_inline_always fp_type asin(fp_type a) {
		// nvidia hardware does not provide hardware instruction to compute asin/acos/atan,
		// so that these must be computed in software.
		//
		// as a fast and accurate approximation in [-0.5, 0.5] use:
		//   EconomizedRationalApproximation[ArcSin[x], {x, {-0.55, 0.55}, 12, 0}]
		//   divided by the "first factor of x" so that we have "1*x"
		//   (note that this isn't being corrected for, but it doesn't matter)
		// =>  x + 0.1666700692808536 x^3 + 0.07487039270444955 x^5 + 0.04641537654451593 x^7 +
		//     0.01979579886701673 x^9 + 0.04922871471335342 x^11
		//
		// for the [-1, -0.5[ and ]0.5, 1] intervals:
		// -> from https://en.wikipedia.org/wiki/List_of_trigonometric_identities
		//    we get: asin(x) = pi/2 - 2 * asin(sqrt((1 - x) / 2))
		// which can be used from 0.5 onwards, or -0.5 downwards:
		// -> (sqrt((1 - 0.5) / 2) = 0.5 => sqrt((1 - 1) / 2) = 0) using this same function
		//    (also see asin(x) in const_math.hpp for comparison)
		//
		// note that nvidia is using something very similar to this, so I'm assuming this should be
		// accurate enough for general usage (+this has a slightly smaller total error than nvidia).
		
		const fp_type abs_a = fabs(a);
		// sqrt(fma(abs_a, -0.5f, 0.5f)) == sqrt((1 - |x|) / 2)
		const fp_type x = (abs_a > fp_type(0.5) ? sqrt(fma(abs_a, fp_type(-0.5), fp_type(0.5))) : a);
		// factored out one x and precompute x^2, then do some nice fma nesting
		const fp_type x_2 = x * x;
		const fp_type asin_0_05 = fma(fma(fma(fma(fma(fp_type(0.04922871471335342), x_2, fp_type(0.01979579886701673)),
												  x_2, fp_type(0.04641537654451593)),
											  x_2, fp_type(0.07487039270444955)),
										  x_2, fp_type(0.1666700692808536)) * x_2, // <- note: doing the *x_2 here,
									  x, x);                                       // <- is more accurate than x * x_2 there
		// since we computed the asin with the absolute x value, need to copy the original sign back in
		return copysign(abs_a > fp_type(0.5) ?
						// pi/2 - 2 * asin(sqrt((1 - |x|) / 2))
						fma(asin_0_05, fp_type(-2.0), const_math::PI_DIV_2<fp_type>) :
						asin_0_05,
						a);
	}
	
	template <typename fp_type, typename = enable_if_t<ext::is_floating_point_v<fp_type>>>
	const_func floor_inline_always fp_type acos(fp_type a) { return const_math::PI_DIV_2<fp_type> - asin(a); }
	
	template <typename fp_type, typename = enable_if_t<ext::is_floating_point_v<fp_type>>>
	const_func floor_inline_always fp_type atan(fp_type a) { return asin(a * rsqrt(a * a + fp_type(1.0))); }
	
	template <typename fp_type, typename = enable_if_t<ext::is_floating_point_v<fp_type>>>
	const_func floor_inline_always fp_type atan2(fp_type y, fp_type x) {
		if(x > fp_type(0.0)) {
			return atan(y / x);
		}
		else if(x < fp_type(0.0)) {
			return atan(y / x) + (y >= fp_type(0.0) ? const_math::PI<fp_type> : -const_math::PI<fp_type>);
		}
		else { // x == 0
			return (y > fp_type(0.0) ? const_math::PI_DIV_2<fp_type> :
					(y < fp_type(0.0) ? -const_math::PI_DIV_2<fp_type> :
					 numeric_limits<fp_type>::quiet_NaN()));
		}
	}
	
	template <typename fp_type, typename = enable_if_t<ext::is_floating_point_v<fp_type>>>
	const_func floor_inline_always fp_type sinh(fp_type a) {
		const auto exp_a = exp(a);
		return fp_type(0.5) * (exp_a - fp_type(1.0) / exp_a); // TODO: check if approx rcp is good enough
	}
	template <typename fp_type, typename = enable_if_t<ext::is_floating_point_v<fp_type>>>
	const_func floor_inline_always fp_type cosh(fp_type a) {
		const auto exp_a = exp(a);
		return fp_type(0.5) * (exp_a + fp_type(1.0) / exp_a);
	}
	template <typename fp_type, typename = enable_if_t<ext::is_floating_point_v<fp_type>>>
	const_func floor_inline_always fp_type tanh(fp_type a) {
		const auto exp_pos = exp(a);
		const auto exp_neg = fp_type(1.0) / exp_pos;
		return (exp_pos - exp_neg) / (exp_pos + exp_neg);
	}
	template <typename fp_type, typename = enable_if_t<ext::is_floating_point_v<fp_type>>>
	const_func floor_inline_always fp_type asinh(fp_type a) {
		return log(a + sqrt(fma(a, a, fp_type(1.0))));
	}
	template <typename fp_type, typename = enable_if_t<ext::is_floating_point_v<fp_type>>>
	const_func floor_inline_always fp_type acosh(fp_type a) {
		return log(a + sqrt(fma(a, a, fp_type(-1.0))));
	}
	template <typename fp_type, typename = enable_if_t<ext::is_floating_point_v<fp_type>>>
	const_func floor_inline_always fp_type atanh(fp_type a) {
		return fp_type(0.5) * log((fp_type(1.0) + a) / (fp_type(1.0) - a));
	}
	
	// non-standard bit counting functions (don't use these directly, use math::func instead)
	const_func floor_inline_always int32_t floor_rt_clz(uint16_t a) {
		return floor_rt_clz((uint32_t)a) - 16 /* upper 16 bits */;
	}
	const_func floor_inline_always int32_t floor_rt_clz(uint32_t a) {
		int32_t ret;
		asm("clz.b32 %0, %1;" : "=r"(ret) : "r"(a));
		return ret;
	}
	const_func floor_inline_always int32_t floor_rt_clz(uint64_t a) {
		int32_t ret;
		asm("clz.b64 %0, %1;" : "=r"(ret) : "l"(a));
		return ret;
	}
	const_func floor_inline_always int32_t floor_rt_ctz(uint16_t a) {
		return floor_rt_ctz(0xFFFF0000u | (uint32_t)a);
	}
	const_func floor_inline_always int32_t floor_rt_ctz(uint32_t a) {
		int32_t ret;
		uint32_t tmp;
		asm("brev.b32 %0, %1;" : "=r"(tmp) : "r"(a));
		asm("clz.b32 %0, %1;" : "=r"(ret) : "r"(tmp));
		return ret;
	}
	const_func floor_inline_always int32_t floor_rt_ctz(uint64_t a) {
		int32_t ret;
		uint64_t tmp;
		asm("brev.b64 %0, %1;" : "=l"(tmp) : "l"(a));
		asm("clz.b64 %0, %1;" : "=r"(ret) : "l"(tmp));
		return ret;
	}
	const_func floor_inline_always int32_t floor_rt_popcount(uint16_t a) {
		return floor_rt_popcount((uint32_t)a);
	}
	const_func floor_inline_always int32_t floor_rt_popcount(uint32_t a) {
		int32_t ret;
		asm("popc.b32 %0, %1;" : "=r"(ret) : "r"(a));
		return ret;
	}
	const_func floor_inline_always int32_t floor_rt_popcount(uint64_t a) {
		int32_t ret;
		asm("popc.b64 %0, %1;" : "=r"(ret) : "l"(a));
		return ret;
	}
	const_func floor_inline_always uint32_t floor_rt_reverse_bits(const uint32_t& value) {
		uint32_t ret;
		asm("brev.b32 %0, %1;" : "=r"(ret) : "r"(value));
		return ret;
	}
	const_func floor_inline_always uint64_t floor_rt_reverse_bits(const uint64_t& value) {
		uint64_t ret;
		asm("brev.b64 %0, %1;" : "=l"(ret) : "l"(value));
		return ret;
	}
	
}

// provided by cuda runtime
extern "C" {
	// NOTE: there is no va_list support in llvm/nvptx, not even via __builtin_*, so emulate it manually via void* -> printf
	extern int vprintf(const char* format, void* vlist);
};

// printf, this builds a local buffer, copies all arguments into it and calls vprintf, which is provided by the hardware
template <typename... Args>
static int printf(const char* format, Args&&... args) {
	alignas(8) uint8_t args_buf[soft_printf::printf_args_total_size<Args...>()];
	uint8_t* args_buf_ptr = &args_buf[0];
	soft_printf::printf_args_apply(soft_printf::no_as::printf_handle_arg(args, &args_buf_ptr)...);
	return vprintf(format, &args_buf);
}

// get_*_id() functions and other id handling
#include <floor/compute/device/cuda_id.hpp>

// barrier and mem_fence functionality
floor_inline_always static void global_barrier() {
	__syncthreads();
}
floor_inline_always static void global_mem_fence() {
	__nvvm_membar_cta();
}
floor_inline_always static void global_read_mem_fence() {
	__nvvm_membar_cta();
}
floor_inline_always static void global_write_mem_fence() {
	__nvvm_membar_cta();
}

floor_inline_always static void local_barrier() {
	__syncthreads();
}
floor_inline_always static void local_mem_fence() {
	__nvvm_membar_cta();
}
floor_inline_always static void local_read_mem_fence() {
	__nvvm_membar_cta();
}
floor_inline_always static void local_write_mem_fence() {
	__nvvm_membar_cta();
}

floor_inline_always static void barrier() {
	__syncthreads();
}

floor_inline_always static void image_barrier() {
	__syncthreads();
}
floor_inline_always static void image_mem_fence() {
	__nvvm_membar_cta();
}
floor_inline_always static void image_read_mem_fence() {
	__nvvm_membar_cta();
}
floor_inline_always static void image_write_mem_fence() {
	__nvvm_membar_cta();
}

// done
#undef FLOOR_CUDA_DIM0
#undef FLOOR_CUDA_DIM1
#undef FLOOR_CUDA_DIM2
#undef FLOOR_CUDA_INVALID
#undef FLOOR_CUDA_DIM_RT

#endif

#endif
