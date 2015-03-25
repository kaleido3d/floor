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

#include <floor/compute/compute_program.hpp>

shared_ptr<compute_kernel> compute_program::get_kernel(const string& func_name) const {
	const auto iter = find(cbegin(kernel_names), cend(kernel_names), func_name);
	if(iter == cend(kernel_names)) return {};
	return kernels[(size_t)distance(cbegin(kernel_names), iter)];
}

shared_ptr<compute_kernel> compute_program::get_kernel_fuzzy(const string& fuzzy_func_name) const {
	const auto iter = find_if(cbegin(kernel_names), cend(kernel_names), [&fuzzy_func_name](const auto& name) {
		return (name.find(fuzzy_func_name) != string::npos);
	});
	if(iter == cend(kernel_names)) return {};
	return kernels[(size_t)distance(cbegin(kernel_names), iter)];
}