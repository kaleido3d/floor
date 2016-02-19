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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/core/platform.hpp>
#include <floor/core/gl_support.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/core/file_io.hpp>
#include <floor/compute/llvm_compute.hpp>
#include <floor/floor/floor.hpp>
#include <floor/floor/floor_version.hpp>

vulkan_compute::vulkan_compute(const vector<string> whitelist) : compute_context() {
	// create a vulkan instance (context)
	const VkApplicationInfo app_info {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = nullptr,
		.pApplicationName = "floor app", // TODO: add setter/getter to floor::
		.applicationVersion = 1, // TODO: add setter/getter to floor::
		.pEngineName = "floor",
		.engineVersion = FLOOR_VERSION_U32,
		// TODO/NOTE: even though the spec allows setting this to 0, nvidias current driver requires VK_API_VERSION / VK_MAKE_VERSION(1, 0, 3)
		.apiVersion = VK_MAKE_VERSION(1, 0, 3),
	};
	// TODO: query exts
	// NOTE: even without surface/xlib extension, this isn't able to start without an x session / headless right now (at least on nvidia drivers)
	static constexpr const char* extensions[] {
		"VK_KHR_surface",
#if 0 // don't use either yet
#if defined(__WINDOWS__)
		"VK_KHR_win32_surface",
#else
		// nvidia only supports this:
		"VK_KHR_xlib_surface",
		// intel only supports this:
		"VK_KHR_xcb_surface",
#endif
#endif
	};
	const VkInstanceCreateInfo instance_info {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pApplicationInfo = &app_info,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = nullptr,
		.enabledExtensionCount = size(extensions),
		.ppEnabledExtensionNames = extensions,
	};
	VK_CALL_RET(vkCreateInstance(&instance_info, nullptr, &ctx), "failed to create vulkan instance");
	
	// get layers
	uint32_t layer_count = 0;
	VK_CALL_RET(vkEnumerateInstanceLayerProperties(&layer_count, nullptr), "failed to retrieve instance layer properties count");
	vector<VkLayerProperties> layers(layer_count);
	if(layer_count > 0) {
		VK_CALL_RET(vkEnumerateInstanceLayerProperties(&layer_count, layers.data()), "failed to retrieve instance layer properties");
	}
	log_debug("found %u vulkan layer%s", layer_count, (layer_count == 1 ? "" : "s"));
	
	// get devices
	uint32_t device_count = 0;
	VK_CALL_RET(vkEnumeratePhysicalDevices(ctx, &device_count, nullptr), "failed to retrieve device count");
	vector<VkPhysicalDevice> queried_devices(device_count);
	VK_CALL_RET(vkEnumeratePhysicalDevices(ctx, &device_count, queried_devices.data()), "failed to retrieve devices");
	log_debug("found %u vulkan device%s", device_count, (device_count == 1 ? "" : "s"));

	auto gpu_counter = (underlying_type_t<compute_device::TYPE>)compute_device::TYPE::GPU0;
	auto cpu_counter = (underlying_type_t<compute_device::TYPE>)compute_device::TYPE::CPU0;
	for(const auto& phys_dev : queried_devices) {
		// get device props and features
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(phys_dev, &props);
		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceFeatures(phys_dev, &features);
		
		// check whitelist
		if(!whitelist.empty()) {
			const auto lc_dev_name = core::str_to_lower(props.deviceName);
			bool found = false;
			for(const auto& entry : whitelist) {
				if(lc_dev_name.find(entry) != string::npos) {
					found = true;
					break;
				}
			}
			if(!found) continue;
		}
		
		// handle device queue info + create queue info, we're going to create as many queues as are allowed by the device
		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &queue_family_count, nullptr);
		if(queue_family_count == 0) {
			log_error("device supports no queue families");
			continue;
		}
		
		vector<VkQueueFamilyProperties> dev_queue_family_props(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &queue_family_count, dev_queue_family_props.data());
		
		// create priorities array once (all set to 0 for now)
		uint32_t max_queue_count = 0;
		for(uint32_t i = 0; i < queue_family_count; ++i) {
			max_queue_count = max(max_queue_count, dev_queue_family_props[i].queueCount);
		}
		if(max_queue_count == 0) {
			log_error("device supports no queues");
			continue;
		}
		const vector<float> priorities(max_queue_count, 0.0f);
		
		vector<VkDeviceQueueCreateInfo> queue_create_info(queue_family_count);
		for(uint32_t i = 0; i < queue_family_count; ++i) {
			auto& queue_info = queue_create_info[i];
			queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_info.pNext = nullptr;
			queue_info.flags = 0;
			queue_info.queueFamilyIndex = i;
			queue_info.queueCount = dev_queue_family_props[i].queueCount;
			queue_info.pQueuePriorities = priorities.data();
		}
		
		// create device
		const VkDeviceCreateInfo dev_info {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.queueCreateInfoCount = queue_family_count,
			.pQueueCreateInfos = queue_create_info.data(),
			.enabledLayerCount = 0,
			.ppEnabledLayerNames = nullptr,
			.enabledExtensionCount = 0,
			.ppEnabledExtensionNames = nullptr,
			.pEnabledFeatures = &features // enable all that is supported
		};
		
		VkDevice dev;
		VK_CALL_CONT(vkCreateDevice(phys_dev, &dev_info, nullptr, &dev), "failed to create device \""s + props.deviceName + "\"");
		
		// add device
		auto device = make_shared<vulkan_device>();
		devices.emplace_back(device);
		physical_devices.emplace_back(phys_dev);
		logical_devices.emplace_back(dev);
		device->physical_device = phys_dev;
		device->device = dev;
		device->name = props.deviceName;
		device->platform_vendor = COMPUTE_VENDOR::KHRONOS; // not sure what to set here
		device->version_str = (to_string(VK_VERSION_MAJOR(props.apiVersion)) + "." +
							   to_string(VK_VERSION_MINOR(props.apiVersion)) + "." +
							   to_string(VK_VERSION_PATCH(props.apiVersion)));
		device->driver_version_str = to_string(props.driverVersion);
		
		if(props.vendorID < 0x10000) {
			switch(props.vendorID) {
				case 0x1002:
					device->vendor = COMPUTE_VENDOR::AMD;
					device->vendor_name = "AMD";
					break;
				case 0x10DE:
					device->vendor = COMPUTE_VENDOR::NVIDIA;
					device->vendor_name = "NVIDIA";
					device->driver_version_str = to_string((props.driverVersion >> 22u) & 0x3FF) + ".";
					device->driver_version_str += to_string((props.driverVersion >> 14u) & 0xFF) + ".";
					device->driver_version_str += to_string((props.driverVersion >> 6u) & 0xFF);
					break;
				case 0x8086:
					device->vendor = COMPUTE_VENDOR::INTEL;
					device->vendor_name = "INTEL";
					break;
				default:
					device->vendor = COMPUTE_VENDOR::UNKNOWN;
					device->vendor_name = "UNKNOWN";
					break;
			}
		}
		else {
			// khronos assigned vendor id (not handling this for now)
			device->vendor = COMPUTE_VENDOR::KHRONOS;
			device->vendor_name = "Khronos assigned vendor";
		}
		
		device->internal_type = props.deviceType;
		switch(props.deviceType) {
			// TODO: differentiate these?
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
				device->type = (compute_device::TYPE)gpu_counter;
				++gpu_counter;
				if(fastest_gpu_device == nullptr) {
					fastest_gpu_device = device;
				}
				break;
			case VK_PHYSICAL_DEVICE_TYPE_CPU:
				device->type = (compute_device::TYPE)cpu_counter;
				++cpu_counter;
				if(fastest_cpu_device == nullptr) {
					fastest_cpu_device = device;
				}
				break;
			case VK_PHYSICAL_DEVICE_TYPE_OTHER:
			default:
				// not handled
				break;
		}
		
		// queue count info
		device->queue_counts.resize(queue_family_count);
		for(uint32_t i = 0; i < queue_family_count; ++i) {
			device->queue_counts[i] = dev_queue_family_props[i].queueCount;
		}
		
		// limits
		const auto& limits = props.limits;
		device->constant_mem_size = limits.maxUniformBufferRange; // not an exact match, but usually the same
		device->local_mem_size = limits.maxComputeSharedMemorySize;
		device->max_work_group_size = limits.maxComputeWorkGroupInvocations;
		device->max_work_item_sizes = { limits.maxComputeWorkGroupCount[0], limits.maxComputeWorkGroupCount[1], limits.maxComputeWorkGroupCount[2] };
		device->max_work_group_item_sizes = { limits.maxComputeWorkGroupSize[0], limits.maxComputeWorkGroupSize[1], limits.maxComputeWorkGroupSize[2] };
		device->max_image_1d_dim = limits.maxImageDimension1D;
		device->max_image_1d_buffer_dim = limits.maxTexelBufferElements;
		device->max_image_2d_dim = { limits.maxImageDimension2D, limits.maxImageDimension2D };
		device->max_image_3d_dim = { limits.maxImageDimension3D, limits.maxImageDimension3D, limits.maxImageDimension3D };
		device->bitness = (limits.sparseAddressSpaceSize > 0xFFFFFFFF ? 64 : 32); // TODO: this is a hack, but there is no other way of doing this

		// retrieve memory info
		VkPhysicalDeviceMemoryProperties mem_props;
		vkGetPhysicalDeviceMemoryProperties(phys_dev, &mem_props);
		
		// global memory (heap with local bit)
		// TODO: should probably use this to figure out memory sizes for integrated gpus (and unified_memory)
		/*for(uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
			if(mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
				device->global_mem_size = mem_props.memoryHeaps[mem_props.memoryTypes[i].heapIndex].size;
				break;
			}
		}*/
		// for now, just assume the correct data is stored in the heap flags
		for(uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
			if(mem_props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
				device->global_mem_size = mem_props.memoryHeaps[i].size;
				device->max_mem_alloc = mem_props.memoryHeaps[i].size; // TODO: min(gpu heap, host heap)?
				break;
			}
		}
		log_msg("max mem alloc: %u bytes / %u MB",
				device->max_mem_alloc,
				device->max_mem_alloc / 1024ULL / 1024ULL);
		log_msg("mem size: %u MB (global), %u KB (local), %u KB (constant)",
				device->global_mem_size / 1024ULL / 1024ULL,
				device->local_mem_size / 1024ULL,
				device->constant_mem_size / 1024ULL);
		
		log_msg("max work-group size: %u", device->max_work_group_size);
		log_msg("max work-group item sizes: %v", device->max_work_group_item_sizes);
		log_msg("max work-item sizes: %v", device->max_work_item_sizes);
		log_msg("queue families: %u", queue_family_count);
		log_msg("max queus (family #0): %u", device->queue_counts[0]);
		
		// TODO: other device flags
		// TODO: fastest device selection, tricky to do without a unit count
		
		// done
		log_debug("%s (Memory: %u MB): %s %s, API: %s, driver: %s",
				  (device->is_gpu() ? "GPU" : (device->is_cpu() ? "CPU" : "UNKNOWN")),
				  (uint32_t)(device->global_mem_size / 1024ull / 1024ull),
				  device->vendor_name,
				  device->name,
				  device->version_str,
				  device->driver_version_str);
	}
	
	// if there are no devices left, init has failed
	if(devices.empty()) {
		if(!queried_devices.empty()) log_warn("no devices left after applying whitelist!");
		return;
	}
	// else: success, we have at least one device
	supported = true;
	
	// workaround non-existent fastest device selection
	fastest_device = devices[0];
}

shared_ptr<compute_queue> vulkan_compute::create_queue(shared_ptr<compute_device> dev) {
	if(dev == nullptr) {
		log_error("nullptr is not a valid device!");
		return {};
	}
	auto vulkan_dev = (vulkan_device*)dev.get();
	
	// can only create a certain amount of queues per device with vulkan, so handle this + handle the queue index
	if(vulkan_dev->cur_queue_idx >= vulkan_dev->queue_counts[0]) {
		log_warn("too many queues were created (max: %u), wrapping around to #0 again", vulkan_dev->queue_counts[0]);
		vulkan_dev->cur_queue_idx = 0;
	}
	const auto next_queue_index = vulkan_dev->cur_queue_idx++;
	
	VkQueue queue_obj;
	const uint32_t family_index = 0; // always family #0 for now
	vkGetDeviceQueue(vulkan_dev->device, family_index, next_queue_index, &queue_obj);
	if(queue_obj == nullptr) {
		log_error("failed to retrieve vulkan device queue");
		return {};
	}
	
	auto ret = make_shared<vulkan_queue>(dev, queue_obj, family_index);
	queues.push_back(ret);
	return ret;
}

// TODO: should probably remove non-device create_buffer/create_image calls ...
shared_ptr<compute_buffer> vulkan_compute::create_buffer(const size_t& size, const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) {
	return make_shared<vulkan_buffer>((vulkan_device*)fastest_device.get(), size, flags, opengl_type);
}

shared_ptr<compute_buffer> vulkan_compute::create_buffer(const size_t& size, void* data, const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) {
	return make_shared<vulkan_buffer>((vulkan_device*)fastest_device.get(), size, data, flags, opengl_type);
}

shared_ptr<compute_buffer> vulkan_compute::create_buffer(shared_ptr<compute_device> device,
														 const size_t& size, const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) {
	return make_shared<vulkan_buffer>((vulkan_device*)device.get(), size, flags, opengl_type);
}

shared_ptr<compute_buffer> vulkan_compute::create_buffer(shared_ptr<compute_device> device,
														 const size_t& size, void* data,
														 const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) {
	return make_shared<vulkan_buffer>((vulkan_device*)device.get(), size, data, flags, opengl_type);
}

shared_ptr<compute_buffer> vulkan_compute::wrap_buffer(shared_ptr<compute_device>, const uint32_t, const uint32_t,
													   const COMPUTE_MEMORY_FLAG) {
	log_error("not supported by vulkan_compute!");
	return {};
}

shared_ptr<compute_buffer> vulkan_compute::wrap_buffer(shared_ptr<compute_device>, const uint32_t, const uint32_t,
													   void*, const COMPUTE_MEMORY_FLAG) {
	log_error("not supported by vulkan_compute!");
	return {};
}

shared_ptr<compute_image> vulkan_compute::create_image(shared_ptr<compute_device> device,
													   const uint4 image_dim,
													   const COMPUTE_IMAGE_TYPE image_type,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<vulkan_image>((vulkan_device*)device.get(), image_dim, image_type, nullptr, flags, opengl_type);
}

shared_ptr<compute_image> vulkan_compute::create_image(shared_ptr<compute_device> device,
													   const uint4 image_dim,
													   const COMPUTE_IMAGE_TYPE image_type,
													   void* data,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<vulkan_image>((vulkan_device*)device.get(), image_dim, image_type, data, flags, opengl_type);
}

shared_ptr<compute_image> vulkan_compute::wrap_image(shared_ptr<compute_device>, const uint32_t, const uint32_t,
													 const COMPUTE_MEMORY_FLAG) {
	log_error("not supported by vulkan_compute!");
	return {};
}

shared_ptr<compute_image> vulkan_compute::wrap_image(shared_ptr<compute_device>, const uint32_t, const uint32_t,
													 void*, const COMPUTE_MEMORY_FLAG) {
	log_error("not supported by vulkan_compute!");
	return {};
}

shared_ptr<vulkan_program> vulkan_compute::add_program(vulkan_program::program_map_type&& prog_map) {
	// create the program object, which in turn will create kernel objects for all kernel functions in the program,
	// for all devices contained in the program map
	auto prog = make_shared<vulkan_program>(move(prog_map));
	{
		GUARD(programs_lock);
		programs.push_back(prog);
	}
	return prog;
}

shared_ptr<compute_program> vulkan_compute::add_program_file(const string& file_name,
															 const string additional_options) {
	// compile the source file for all devices in the context
	vulkan_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for(const auto& dev : devices) {
		prog_map.insert_or_assign((vulkan_device*)dev.get(),
								  create_vulkan_program(dev, llvm_compute::compile_program_file(dev, file_name, additional_options,
																								llvm_compute::TARGET::SPIRV)));
	}
	return add_program(move(prog_map));
}

shared_ptr<compute_program> vulkan_compute::add_program_source(const string& source_code,
															   const string additional_options) {
	// compile the source code for all devices in the context
	vulkan_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for(const auto& dev : devices) {
		prog_map.insert_or_assign((vulkan_device*)dev.get(),
								  create_vulkan_program(dev, llvm_compute::compile_program(dev, source_code, additional_options,
																						   llvm_compute::TARGET::SPIRV)));
	}
	return add_program(move(prog_map));
}

vulkan_program::vulkan_program_entry vulkan_compute::create_vulkan_program(shared_ptr<compute_device> device,
																		   pair<string, vector<llvm_compute::kernel_info>> program_data) {
	// TODO: implement this
	log_error("not yet supported by vulkan_compute!");
	return {};
}

shared_ptr<compute_program> vulkan_compute::add_precompiled_program_file(const string& file_name floor_unused,
																		 const vector<llvm_compute::kernel_info>& kernel_infos floor_unused) {
	// TODO: !
	log_error("not yet supported by vulkan_compute!");
	return {};
}

shared_ptr<compute_program::program_entry> vulkan_compute::create_program_entry(shared_ptr<compute_device> device,
																				pair<string, vector<llvm_compute::kernel_info>> program_data) {
	return make_shared<vulkan_program::vulkan_program_entry>(create_vulkan_program(device, program_data));
}

#endif