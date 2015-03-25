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

#ifndef __FLOOR_OPENCL_COMPUTE_HPP__
#define __FLOOR_OPENCL_COMPUTE_HPP__

#include <floor/compute/opencl/opencl_common.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/compute/compute_base.hpp>
#include <floor/compute/opencl/opencl_buffer.hpp>
#include <floor/compute/opencl/opencl_device.hpp>
#include <floor/compute/opencl/opencl_kernel.hpp>
#include <floor/compute/opencl/opencl_program.hpp>
#include <floor/compute/opencl/opencl_queue.hpp>

//! TODO
class opencl_compute final : public compute_base {
public:
	//////////////////////////////////////////
	// init / context creation
	
	~opencl_compute() override {}
	
	void init(const bool use_platform_devices = false,
			  const uint32_t platform_index = ~0u,
			  const bool gl_sharing = false,
			  const unordered_set<string> device_restriction = {}) override;
	
	bool is_supported() const override { return supported; }
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::OPENCL; }
	
	//////////////////////////////////////////
	// device functions
	
	shared_ptr<compute_queue> create_queue(shared_ptr<compute_device> dev) override;
	
	//////////////////////////////////////////
	// buffer creation
	
	shared_ptr<compute_buffer> create_buffer(const size_t& size,
											 const COMPUTE_BUFFER_FLAG flags = (COMPUTE_BUFFER_FLAG::READ_WRITE |
																				COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) override;
	
	shared_ptr<compute_buffer> create_buffer(const size_t& size,
											 void* data,
											 const COMPUTE_BUFFER_FLAG flags = (COMPUTE_BUFFER_FLAG::READ_WRITE |
																				COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) override;
	
	shared_ptr<compute_buffer> create_buffer(shared_ptr<compute_device> device,
											 const size_t& size,
											 const COMPUTE_BUFFER_FLAG flags = (COMPUTE_BUFFER_FLAG::READ_WRITE |
																				COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) override;
	
	shared_ptr<compute_buffer> create_buffer(shared_ptr<compute_device> device,
											 const size_t& size,
											 void* data,
											 const COMPUTE_BUFFER_FLAG flags = (COMPUTE_BUFFER_FLAG::READ_WRITE |
																				COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) override;
	
	//////////////////////////////////////////
	// basic control functions
	
	void finish() override;
	void flush() override;
	void activate_context() override;
	void deactivate_context() override;
	
	//////////////////////////////////////////
	// program/kernel functionality
	
	shared_ptr<compute_program> add_program_file(const string& file_name,
												 const string additional_options = "") override;
	
	shared_ptr<compute_program> add_program_source(const string& source_code,
												   const string additional_options = "") override;
	
	shared_ptr<compute_program> add_precompiled_program_file(const string& file_name,
															 const vector<llvm_compute::kernel_info>& kernel_infos) override;
	
protected:
	cl_context ctx { nullptr };
	vector<cl_device_id> ctx_devices;
	
	OPENCL_VERSION platform_cl_version { OPENCL_VERSION::OPENCL_1_0 };
	
	vector<cl_image_format> image_formats;
	
	vector<shared_ptr<opencl_program>> programs;
	
};

#endif

#endif