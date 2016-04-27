/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#ifndef __FLOOR_OPENCL_KERNEL_HPP__
#define __FLOOR_OPENCL_KERNEL_HPP__

#include <floor/compute/opencl/opencl_common.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/core/logger.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/compute/opencl/opencl_buffer.hpp>
#include <floor/compute/opencl/opencl_image.hpp>

// the amount of macro voodoo is too damn high ...
#define FLOOR_OPENCL_KERNEL_IMPL 1
#include <floor/compute/compute_kernel.hpp>
#undef FLOOR_OPENCL_KERNEL_IMPL

class opencl_device;
class opencl_kernel final : public compute_kernel {
public:
	struct opencl_kernel_entry : kernel_entry {
		cl_kernel kernel { nullptr };
	};
	typedef flat_map<opencl_device*, opencl_kernel_entry> kernel_map_type;
	
	opencl_kernel(kernel_map_type&& kernels);
	~opencl_kernel() override;
	
	template <typename... Args> void execute(compute_queue* queue,
											 const uint32_t work_dim,
											 const uint3 global_work_size,
											 const uint3 local_work_size_,
											 Args&&... args) REQUIRES(!args_lock) {
		// find entry for queue device
		const auto kernel_iter = get_kernel(queue);
		if(kernel_iter == kernels.cend()) {
			log_error("no kernel for this compute queue/device exists!");
			return;
		}
		
		// check work size
		const uint3 local_work_size = check_local_work_size(kernel_iter->second, local_work_size_);
		
		// need to make sure that only one thread is setting kernel arguments at a time
		GUARD(args_lock);
		
		// set and handle kernel arguments
		set_kernel_arguments<0>(kernel_iter->second, forward<Args>(args)...);
		
		// run
		execute_internal(queue, kernel_iter->second, work_dim, global_work_size, local_work_size);
	}
	
	const kernel_entry* get_kernel_entry(shared_ptr<compute_device> dev) const override {
		const auto ret = kernels.get((opencl_device*)dev.get());
		return !ret.first ? nullptr : &ret.second->second;
	}
	
protected:
	const kernel_map_type kernels;
	flat_map<const kernel_entry*, bool> warn_map;
	
	atomic_spin_lock args_lock;
	
	typename kernel_map_type::const_iterator get_kernel(const compute_queue* queue) const;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::OPENCL; }
	
	void execute_internal(compute_queue* queue,
						  const opencl_kernel_entry& entry,
						  const uint32_t& work_dim,
						  const uint3& global_work_size,
						  const uint3& local_work_size) const;
	
	//! handle kernel call terminator
	template <cl_uint num>
	floor_inline_always void set_kernel_arguments(const opencl_kernel_entry&) const {}
	
	//! set kernel argument and recurse
	template <cl_uint num, typename T, typename... Args>
	floor_inline_always void set_kernel_arguments(const opencl_kernel_entry& entry,
												  T&& arg, Args&&... args) const {
		set_kernel_argument(entry, num, forward<T>(arg));
		set_kernel_arguments<num + 1>(entry, forward<Args>(args)...);
	}
	
	//! actual kernel argument setter
	template <typename T, enable_if_t<!is_pointer<T>::value>* = nullptr>
	floor_inline_always void set_kernel_argument(const opencl_kernel_entry& entry,
												 const cl_uint num, T&& arg) const {
		CL_CALL_RET(clSetKernelArg(entry.kernel, num, sizeof(T), &arg),
					"failed to set generic kernel argument");
	}
	
	floor_inline_always void set_kernel_argument(const opencl_kernel_entry& entry,
												 const cl_uint num, shared_ptr<compute_buffer> arg) const {
		set_kernel_argument(entry, num, arg.get());
	}
	
	floor_inline_always void set_kernel_argument(const opencl_kernel_entry& entry,
												 const cl_uint num, shared_ptr<compute_image> arg) const {
		set_kernel_argument(entry, num, arg.get());
	}
	
	floor_inline_always void set_kernel_argument(const opencl_kernel_entry& entry,
												 const cl_uint num, const compute_buffer* arg) const {
		CL_CALL_RET(clSetKernelArg(entry.kernel, num, sizeof(cl_mem),
								   &((opencl_buffer*)arg)->get_cl_buffer()),
					"failed to set buffer kernel argument");
	}
	
	floor_inline_always void set_kernel_argument(const opencl_kernel_entry& entry,
												 const cl_uint num, const compute_image* arg) const {
		CL_CALL_RET(clSetKernelArg(entry.kernel, num, sizeof(cl_mem),
								   &((opencl_image*)arg)->get_cl_image()),
					"failed to set image kernel argument");
	}
	
};

#endif

#endif
