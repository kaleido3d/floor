/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#ifndef __FLOOR_METAL_COMMON_HPP__
#define __FLOOR_METAL_COMMON_HPP__

#include <floor/core/essentials.hpp>
#include <cstdint>

//! metal version of the platform/driver/device
enum class METAL_VERSION : uint32_t {
	NONE,
	// NOTE: Metal 1.1 is the min supported version right now
	METAL_1_1,
	METAL_1_2,
	METAL_2_0,
	METAL_2_1,
	METAL_2_2,
	METAL_2_3,
};

constexpr const char* metal_version_to_string(const METAL_VERSION& version) {
	switch(version) {
		case METAL_VERSION::NONE: return "";
		case METAL_VERSION::METAL_1_1: return "1.1";
		case METAL_VERSION::METAL_1_2: return "1.2";
		case METAL_VERSION::METAL_2_0: return "2.0";
		case METAL_VERSION::METAL_2_1: return "2.1";
		case METAL_VERSION::METAL_2_2: return "2.2";
		case METAL_VERSION::METAL_2_3: return "2.3";
	}
}
constexpr const char* metal_major_version_to_string(const METAL_VERSION& version) {
	switch(version) {
		case METAL_VERSION::NONE: return "";
		case METAL_VERSION::METAL_1_1:
		case METAL_VERSION::METAL_1_2: return "1";
		case METAL_VERSION::METAL_2_0:
		case METAL_VERSION::METAL_2_1:
		case METAL_VERSION::METAL_2_2:
		case METAL_VERSION::METAL_2_3: return "2";
	}
}
constexpr const char* metal_minor_version_to_string(const METAL_VERSION& version) {
	switch(version) {
		case METAL_VERSION::NONE: return "";
		case METAL_VERSION::METAL_1_1: return "1";
		case METAL_VERSION::METAL_1_2: return "2";
		case METAL_VERSION::METAL_2_0: return "0";
		case METAL_VERSION::METAL_2_1: return "1";
		case METAL_VERSION::METAL_2_2: return "2";
		case METAL_VERSION::METAL_2_3: return "3";
	}
}
constexpr METAL_VERSION metal_version_from_uint(const uint32_t major, const uint32_t minor) {
	if (major == 0 || major > 3) return METAL_VERSION::NONE;
	if (major == 1) {
		switch (minor) {
			case 1: return METAL_VERSION::METAL_1_1;
			case 2: return METAL_VERSION::METAL_1_2;
			case 0:
			default: return METAL_VERSION::NONE;
		}
	} else if (major == 2) {
		// major == 2
		switch (minor) {
			case 0: return METAL_VERSION::METAL_2_0;
			case 1: return METAL_VERSION::METAL_2_1;
			case 2: return METAL_VERSION::METAL_2_2;
			case 3: return METAL_VERSION::METAL_2_3;
			default: return METAL_VERSION::NONE;
		}
	}
	return METAL_VERSION::NONE;
}

#if !defined(FLOOR_NO_METAL)

#endif // FLOOR_NO_METAL

#endif
