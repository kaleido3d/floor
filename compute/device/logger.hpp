/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_LOGGER_HPP__
#define __FLOOR_COMPUTE_DEVICE_LOGGER_HPP__

#include <floor/constexpr/const_string.hpp>
#include <floor/constexpr/const_array.hpp>
#include <floor/core/cpp_ext.hpp>
#include <tuple>

// silence clang warnings about non-literal format strings - while it might be right that
// these aren't proper c string literals, this whole thing wouldn't work if the input literals weren't
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

// "make_const_string" for strings in constant address space
template <size_t n> constexpr auto make_constant_string(const constant char (&str)[n]) {
	return const_string<n> { make_sized_array<n>((const char*)str) };
}

// helper function to determine if a type is a floor vector*
template <typename any_type, typename = void> struct is_floor_vector : public false_type {};
template <typename vec_type> struct is_floor_vector<vec_type, enable_if_t<decay_t<vec_type>::dim >= 1 && decay_t<vec_type>::dim <= 4>> : public true_type {};

// (don't look at the man behind the curtain ...)
#define print(str, ...) ({ \
	typedef decltype(device_logger::create_tupled_args(__VA_ARGS__)) tupled_args_type; \
	constexpr auto arg_types = device_logger::process_args<device_logger::str_dollar_count(str)>(tupled_args_type {}); \
	constexpr constant const auto pstr = device_logger::make_printf_string<make_constant_string(str).hash(), \
																		   device_logger::compute_expanded_len(arg_types)>(str, arg_types); \
	device_logger::log(pstr.content.data, ##__VA_ARGS__); \
})


// compute device logging functions
class device_logger {
public:
	// internal representation of format types
	enum class ARG_TYPE : uint32_t {
		// lowest 8-bit: type
		__TYPE_SHIFT = 0u,
		__TYPE_MASK = 0xFFu,
		INVALID = 0,
		INT,
		UINT,
		INT64,
		UINT64,
		STRING,
		FLOAT,
		DOUBLE,
		VEC,
		__MAX_TYPE = VEC,
		
		// next 8-bit: type specifics
		__SPEC_SHIFT = 8u,
		__SPEC_MASK = 0xFF00u,
		VEC1 = 1u << __SPEC_SHIFT,
		VEC2 = 2u << __SPEC_SHIFT,
		VEC3 = 3u << __SPEC_SHIFT,
		VEC4 = 4u << __SPEC_SHIFT,
		
		// upper 16-bit: additional type specifics
		// for VEC*: contains the component type
		__ADD_SPEC_SHIFT = 16u,
		__ADD_SPEC_MASK = 0xFFFF0000u,
	};
	static_assert(uint32_t(ARG_TYPE::__MAX_TYPE) <= 0xFFu, "too many types");
	floor_enum_ext(ARG_TYPE)
	
	// helper function to create a type tuple of all argument types
	// (this is necessary, because non-constexpr args can't be touched directly with constexpr!)
	template <typename... Args>
	static constexpr auto create_tupled_args(Args&&...) {
		return tuple<decay_t<Args>...> {};
	}
	
	// type -> ARG_TYPE
	template <typename T, typename = void> struct handle_arg_type {
		// will error on unhandled types later on
		static constexpr ARG_TYPE type() { return ARG_TYPE::INVALID; }
	};
	template <typename T> struct handle_arg_type<T, enable_if_t<(is_integral<T>::value &&
																 is_signed<T>::value &&
																 !is_pointer<T>::value)>> {
		static constexpr ARG_TYPE type() {
			return (sizeof(T) <= 4 ? ARG_TYPE::INT : ARG_TYPE::INT64);
		}
	};
	template <typename T> struct handle_arg_type<T, enable_if_t<(is_integral<T>::value &&
																 !is_signed<T>::value &&
																 !is_pointer<T>::value)>> {
		static constexpr ARG_TYPE type() {
			return (sizeof(T) <= 4 ? ARG_TYPE::UINT : ARG_TYPE::UINT64);
		}
	};
	template <typename T> struct handle_arg_type<T, enable_if_t<is_floating_point<T>::value>> {
		static constexpr ARG_TYPE type() { return (sizeof(T) == 4 ? ARG_TYPE::FLOAT : ARG_TYPE::DOUBLE); }
	};
	template <typename T> struct handle_arg_type<T, enable_if_t<(is_pointer<T>::value &&
																 (is_same<T, char*>::value ||
																  is_same<T, constant char*>::value ||
																  is_same<T, const char*>::value ||
																  is_same<T, const constant char*>::value))>> {
		static constexpr ARG_TYPE type() { return ARG_TYPE::STRING; }
	};
	template <typename T> struct handle_arg_type<T, enable_if_t<is_floor_vector<T>::value>> {
		static constexpr ARG_TYPE type() {
			ARG_TYPE ret = ARG_TYPE::VEC;
			switch(T::dim) {
				case 1: ret |= ARG_TYPE::VEC1; break;
				case 2: ret |= ARG_TYPE::VEC2; break;
				case 3: ret |= ARG_TYPE::VEC3; break;
				case 4: ret |= ARG_TYPE::VEC4; break;
				default: return ARG_TYPE::INVALID;
			}
			const auto scalar_arg_type = handle_arg_type<typename T::decayed_scalar_type>::type();
			if(scalar_arg_type == ARG_TYPE::INVALID) return ARG_TYPE::INVALID;
			ret |= (ARG_TYPE)(uint32_t(scalar_arg_type) << uint32_t(ARG_TYPE::__ADD_SPEC_SHIFT));
			return ret;
		}
	};
	
	//
	template <size_t array_len>
	static constexpr void handle_arg_types(const_array<ARG_TYPE, array_len>& arg_types, size_t cur_idx) {
		// terminate
		arg_types[cur_idx] = ARG_TYPE::INVALID;
	}
	
	template <typename T, typename... Args, size_t array_len>
	static constexpr void handle_arg_types(const_array<ARG_TYPE, array_len>& arg_types, size_t cur_idx) {
		typedef decay_t<T> decayed_type;
		static_assert(handle_arg_type<decayed_type>::type() != ARG_TYPE::INVALID, "unsupported type");
		arg_types[cur_idx] = handle_arg_type<decayed_type>::type();
		handle_arg_types<Args...>(arg_types, cur_idx + 1);
	}
	
	// creates the array of argument types necessary to create the format string
	template<size_t dollar_count, typename... Args>
	static constexpr auto process_args(tuple<Args...> dummy_tupple) {
		// counts must match (fail early if not)
		static_assert(tuple_size<decltype(dummy_tupple)>() == dollar_count, "invalid arg count");
		
		// map types to ARG_TYPEs
		const_array<ARG_TYPE, dollar_count + 1> arg_types {};
		handle_arg_types<Args...>(arg_types, 0);
		return arg_types;
	}
	
	// count '$' symbols
	template<size_t len>
	static constexpr size_t str_dollar_count(const constant char (&str)[len]) {
		size_t dollar_count = 0;
		for(size_t i = 0; i < len; ++i) {
			if(str[i] == '$') ++dollar_count;
		}
		return dollar_count;
	}
	
	// computes the additional storage requirements due to the format string
	template<size_t array_len>
	static constexpr size_t compute_expanded_len(const const_array<ARG_TYPE, array_len>& arg_types) {
		size_t ret = 0;
		// iterate over arg types (-1, b/c it is terminated by an additional INVALID type to properly handle "no args")
		for(size_t i = 0; i < (array_len - 1); ++i) {
			switch(arg_types[i] & ARG_TYPE::__TYPE_MASK) {
				case ARG_TYPE::INT:
				case ARG_TYPE::UINT:
				case ARG_TYPE::FLOAT:
				case ARG_TYPE::DOUBLE:
				case ARG_TYPE::STRING:
					++ret; // d/u/f/s
					break;
				case ARG_TYPE::INT64:
				case ARG_TYPE::UINT64:
					ret += 3; // lld/llu
					break;
				case ARG_TYPE::VEC: {
					// vector component count
					const auto count = (uint32_t(arg_types[i] & ARG_TYPE::__SPEC_MASK) >>
										uint32_t(ARG_TYPE::__SPEC_SHIFT));
					// vector component type
					size_t type_size = 0;
					switch((ARG_TYPE)(uint32_t(arg_types[i] & ARG_TYPE::__ADD_SPEC_MASK) >>
									  uint32_t(ARG_TYPE::__ADD_SPEC_SHIFT))) {
						case ARG_TYPE::INT:
						case ARG_TYPE::UINT:
						case ARG_TYPE::FLOAT:
						case ARG_TYPE::DOUBLE:
						case ARG_TYPE::STRING:
							type_size = 2; // %*
							break;
						case ARG_TYPE::INT64:
						case ARG_TYPE::UINT64:
							type_size = 4; // %ll*
							break;
						default:
							type_size = 0;
							break;
					}
					// (count - 1) * 2 for ", "; +2 for parentheses "(comp, ...)"; -1 for initial '$'
					ret += count * type_size + (count - 1) * 2u + 2u - 1u;
				} break;
				case ARG_TYPE::INVALID:
				default:
					break;
			}
		}
		return ret;
	}
	
	// creates the actual printf format string
	template<uint32_t hash, size_t expanded_len, size_t len, size_t array_len>
	static constexpr auto make_printf_string(const constant char (&str)[len],
											 const const_array<ARG_TYPE, array_len>& arg_types) {
		// +1 for '\n'
		char pstr[len + expanded_len + 1] {};
		
		size_t arg_num = 0, str_idx = 0, out_idx = 0;
		for(; str_idx < (len - 1); ++str_idx) {
			if(str[str_idx] == '$') {
				const auto arg_type = (arg_types[arg_num] & ARG_TYPE::__TYPE_MASK);
				if(arg_type == ARG_TYPE::INVALID ||
				   arg_type > ARG_TYPE::__MAX_TYPE) {
					// invalid, replace with whitespace (shouldn't happen)
					pstr[out_idx++] = ' ';
				}
				else {
					if(arg_type != ARG_TYPE::VEC) pstr[out_idx++] = '%';
					switch(arg_type) {
						case ARG_TYPE::INT:
							pstr[out_idx++] = 'd';
							break;
						case ARG_TYPE::UINT:
							pstr[out_idx++] = 'u';
							break;
						case ARG_TYPE::FLOAT:
						case ARG_TYPE::DOUBLE:
							pstr[out_idx++] = 'f';
							break;
						case ARG_TYPE::STRING:
							pstr[out_idx++] = 's';
							break;
						case ARG_TYPE::INT64:
							pstr[out_idx++] = 'l';
							pstr[out_idx++] = 'l';
							pstr[out_idx++] = 'd';
							break;
						case ARG_TYPE::UINT64:
							pstr[out_idx++] = 'l';
							pstr[out_idx++] = 'l';
							pstr[out_idx++] = 'u';
							break;
						case ARG_TYPE::VEC: {
							pstr[out_idx++] = '(';
							const auto count = (uint32_t(arg_types[arg_num] & ARG_TYPE::__SPEC_MASK) >>
												uint32_t(ARG_TYPE::__SPEC_SHIFT));
							const auto comp_type = (ARG_TYPE)(uint32_t(arg_types[arg_num] & ARG_TYPE::__ADD_SPEC_MASK) >>
															  uint32_t(ARG_TYPE::__ADD_SPEC_SHIFT));
							for(size_t i = 0; i < count; ++i) {
								if(comp_type == ARG_TYPE::INVALID ||
								   comp_type > ARG_TYPE::__MAX_TYPE) {
									continue;
								}
								pstr[out_idx++] = '%';
								switch(comp_type) {
									case ARG_TYPE::INT:
										pstr[out_idx++] = 'd';
										break;
									case ARG_TYPE::UINT:
										pstr[out_idx++] = 'u';
										break;
									case ARG_TYPE::FLOAT:
									case ARG_TYPE::DOUBLE:
										pstr[out_idx++] = 'f';
										break;
									case ARG_TYPE::STRING:
										pstr[out_idx++] = 's';
										break;
									case ARG_TYPE::INT64:
										pstr[out_idx++] = 'l';
										pstr[out_idx++] = 'l';
										pstr[out_idx++] = 'd';
										break;
									case ARG_TYPE::UINT64:
										pstr[out_idx++] = 'l';
										pstr[out_idx++] = 'l';
										pstr[out_idx++] = 'u';
										break;
									default: break;
								}
								
								if(i < (count - 1)) {
									pstr[out_idx++] = ',';
									pstr[out_idx++] = ' ';
								}
							}
							pstr[out_idx++] = ')';
						} break;
						default: break;
					}
				}
				++arg_num;
			}
			else {
				// just copy
				pstr[out_idx++] = str[str_idx];
			}
		}
		pstr[out_idx++] = '\n';
		pstr[out_idx++] = '\0';
		
		return make_const_string(pstr);
	}
	
	template <typename T, enable_if_t<!is_floor_vector<T>::value, int> = 0>
	static constexpr auto tupled_arg(T&& arg) {
		return tuple<const T&>(std::cref(arg));
	}
	template <typename T, enable_if_t<is_floor_vector<T>::value, int> = 0>
	static constexpr auto tupled_arg(T&& vec) {
		return vec.as_tuple_ref();
	}
	
	// final call: forward to printf
	template <typename... Args>
	static void log(const constant char* format, Args&&... args) {
#if !defined(FLOOR_COMPUTE_CUDA)
		apply(printf, tuple_cat(tie(format), tupled_arg(forward<Args>(args))...));
#else
		// TODO: need a different approach for cuda, because printf type can't be infered,
		// because it's a variadic template and not a variadic c function
#endif
	}
	
};

// cleanup
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif