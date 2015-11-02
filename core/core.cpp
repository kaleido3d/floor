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

#include <floor/core/core.hpp>
#include <floor/core/unicode.hpp>
#include <thread>

#if (defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__))
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if !defined(FLOOR_IOS)
#include <cpuid.h>
#endif

// TODO: add thread safety for gen and rd?
random_device core::rd;
mt19937 core::gen(core::rd());

void core::init() {
}

/*! converts (projects) a 3d vertex to a 2d screen position
 *  @param v the 3d vertex
 *  @param p the 2d screen position
 */
int2 core::get_2d_from_3d(const float3& vec, const matrix4f& mview, const matrix4f& mproj, const int4& viewport) {
	// transform vector (*TMVP)
	const float3 mview_vec = vec * mview; // save tmp result, needed later
	float3 proj_vec = mview_vec * mproj;
	
	// check if point is behind cam / "invisible"
	if(mview_vec.z >= 0.0f) {
		return int2 { numeric_limits<int>::min() };
	}
	
	proj_vec *= -1.0f / mview_vec.z;
	
	// and finally: compute window position
	return {
		(int)(viewport[2] * (proj_vec.x * 0.5f + 0.5f) + viewport[0]),
		(int)(viewport[3] - viewport[3] * (proj_vec.y * 0.5f + 0.5f) + viewport[1]) // flip y
	};
}

float3 core::get_3d_from_2d(const uint2& p, const matrix4f& mview, const matrix4f& mproj, const int4& viewport) {
	const matrix4f ipm((mview * mproj).invert());
	const float3 wnd_vec((((p.x - float(viewport[0])) * 2.0f) / float(viewport[2])) - 1.0f,
						 (((p.y - float(viewport[1])) * 2.0f) / float(viewport[3])) - 1.0f,
						 1.0f);
	return (wnd_vec * ipm);
}

string core::find_and_replace(const string& str, const string& find, const string& repl) {
	string ret = str;
	find_and_replace(ret, find, repl);
	return ret;
}

void core::find_and_replace(string& str, const string& find, const string& repl) {
	// consecutive search and replace routine
	const size_t find_len = find.size();
	const size_t replace_len = repl.size();
	if(find_len == 0) return; // replace_len might be 0 (if it's an empty string -> "")
	
	size_t pos, old_pos = 0;
	while((pos = str.find(find, old_pos)) != string::npos) {
		str.replace(pos, find_len, repl.c_str(), replace_len);
		old_pos = pos + replace_len;
	}
}

string core::find_and_replace_once(const string& str, const string& find_str, const string& repl_str, const size_t start_pos) {
	string ret = str;
	find_and_replace_once(ret, find_str, repl_str, start_pos);
	return ret;
}

void core::find_and_replace_once(string& str, const string& find_str, const string& repl_str, const size_t start_pos) {
	const size_t find_len = find_str.size();
	const size_t replace_len = repl_str.size();
	if(find_len == 0) return;
	
	size_t pos;
	if((pos = str.find(find_str, start_pos)) != string::npos) {
		str.replace(pos, find_len, repl_str.c_str(), replace_len);
	}
}

#define tokenize_algorithm(delim_size) \
vector<string> dst; \
size_t pos = 0; \
size_t old_pos = 0; \
if(src.find(delim, pos) != string::npos) { \
	while((pos = src.find(delim, old_pos)) != string::npos) { \
		dst.emplace_back(src.substr(old_pos, pos - old_pos)); \
		old_pos = pos + delim_size; \
	} \
	dst.emplace_back(src.substr(old_pos, pos - old_pos)); \
} \
else dst.emplace_back(src); \
return dst;

vector<string> core::tokenize(const string& src, const char& delim) {
	tokenize_algorithm(1)
}

vector<string> core::tokenize(const string& src, const string& delim) {
	const size_t delim_size = delim.size();
	tokenize_algorithm(delim_size)
}

void core::str_to_lower_inplace(string& str) {
	transform(str.begin(), str.end(), str.begin(), ptr_fun(::tolower));
}

void core::str_to_upper_inplace(string& str) {
	transform(str.begin(), str.end(), str.begin(), ptr_fun(::toupper));
}

string core::str_to_lower(const string& str) {
	string ret;
	ret.resize(str.length());
	transform(str.begin(), str.end(), ret.begin(), ptr_fun(::tolower));
	return ret;
}

string core::str_to_upper(const string& str) {
	string ret;
	ret.resize(str.length());
	transform(str.begin(), str.end(), ret.begin(), ptr_fun(::toupper));
	return ret;
}

string core::strip_path(const string& in_path) {
	string path = in_path;
	size_t pos = 0, erase_pos;
	while((pos = path.find("../", 0)) != string::npos && pos != 0) {
		erase_pos = path.rfind("/", pos-2);
#if defined(__WINDOWS__)
		if(erase_pos == string::npos) erase_pos = path.rfind("\\", pos-2);
#endif
		if(erase_pos != string::npos) {
			path.erase(erase_pos+1, pos+2-erase_pos);
		}
	}
	
#if defined(__WINDOWS__) // additional windows path handling
	pos = 0;
	while((pos = path.find("..\\", 0)) != string::npos && pos != 0) {
		erase_pos = path.rfind("/", pos-2);
		if(erase_pos == string::npos) erase_pos = path.rfind("\\", pos-2);
		if(erase_pos != string::npos) {
			path.erase(erase_pos+1, pos+2-erase_pos);
		}
	}
#endif
	
	if(path[path.length()-1] != '/' && path[path.length()-1] != '\\') {
		pos = path.rfind("/");
		if(pos == string::npos) pos = path.rfind("\\");
		if(pos == string::npos) {
			path = "/"; // sth is wrong?
		}
		else {
			path = path.substr(0, pos+1);
		}
	}
	
	return path;
}

string core::strip_filename(const string& in_path) {
	string filename = in_path;
	const auto slash_pos = in_path.rfind('/');
	if(slash_pos != string::npos) {
		filename = filename.substr(slash_pos + 1);
	}
	
#if defined(__WINDOWS__)
	const auto backslash_pos = in_path.rfind('\\');
	if(backslash_pos != string::npos) {
		filename = filename.substr(backslash_pos + 1);
	}
#endif
	
	return filename;
}

string core::trim(const string& str) {
	if(str.length() == 0) return "";
	
	string ret = str;
	size_t pos = 0;
	while(pos < ret.length() && (ret[pos] == ' ' || ret[pos] == '\t' || ret[pos] == '\r' || ret[pos] == '\n')) {
		ret.erase(pos, 1);
	}
	
	if(ret.length() > 0) {
		pos = ret.length()-1;
		while(pos < ret.length() && (ret[pos] == ' ' || ret[pos] == '\t' || ret[pos] == '\r' || ret[pos] == '\n')) {
			ret.erase(pos, 1);
			if(pos > 0) pos--;
		}
	}
	return ret;
}

string core::escape_string(const string& str) {
	if(str.size() == 0) return str;
	
	string ret = str;
	size_t pos = 0;
	for(;;) {
		if((pos = ret.find_first_of("\'\"\\", pos)) == string::npos) break;
		ret.insert(pos, "\\");
		pos += 2;
	}
	return ret;
}

map<string, file_io::FILE_TYPE> core::get_file_list(const string& directory,
													const string file_extension,
													const bool always_get_folders) {
	map<string, file_io::FILE_TYPE> file_list;
	string dir_str = directory;
	size_t pos;

	// get file list (os specific)
#if defined(__WINDOWS__)
	struct _finddata_t c_file;
	intptr_t h_file;
	
	dir_str += "*";
	if((h_file = _findfirst(dir_str.c_str(), &c_file)) != -1L) {
		do {
			const string name = c_file.name;
			const bool is_folder = (c_file.attrib & _A_SUBDIR);
			if(file_extension != "" && (!is_folder || (is_folder && !always_get_folders))) {
				if((pos = name.rfind(".")) != string::npos) {
					if(name.substr(pos+1, name.size()-pos-1) != file_extension) continue;
				}
				else continue; // no extension
			}
			
			if(is_folder) file_list[name] = file_io::FILE_TYPE::DIR;
			else file_list[name] = file_io::FILE_TYPE::NONE;
		}
		while(_findnext(h_file, &c_file) == 0);
		
		_findclose(h_file);
	}
#else // works under Linux, OS X, FreeBSD (and probably all other posix systems)
	struct dirent** namelist = nullptr;
	
	dir_str += ".";
	int n = scandir(dir_str.c_str(), &namelist, 0, alphasort);
	
	for(int j = 1; j < n; j++) {
#if !defined(__APPLE__)
		// TODO: this probably needs some conversion as well, but there is no way to find out using only posix functions ...
		const string name = namelist[j]->d_name;
#else
		const string name = unicode::utf8_decomp_to_precomp(namelist[j]->d_name);
#endif
		const bool is_folder = (namelist[j]->d_type == 4);
		if(file_extension != "" && (!is_folder || (is_folder && !always_get_folders))) {
			if((pos = name.rfind(".")) != string::npos) {
				if(name.substr(pos+1, name.size()-pos-1) != file_extension) continue;
			}
			else continue; // no extension
		}

		// TODO: use sys/stat.h instead (glibc has some issues where DT_DIR is not defined or recursively-self-defined ...)
		// note: 4 == DT_DIR
		if(is_folder) file_list[name] = file_io::FILE_TYPE::DIR;
		else file_list[name] = file_io::FILE_TYPE::NONE;
	}
	
	if(namelist != nullptr) {
		for(int i = 0; i < n; i++) {
			free(namelist[i]);
		}
		free(namelist);
	}
#endif
	
	return file_list;
}

/*! computes the normal of a triangle specified by v1, v2 and v3
 *  @param v1 the first vertex
 *  @param v2 the second vertex
 *  @param v3 the third vertex
 *  @param normal the "output" normal
 */
void core::compute_normal(const float3& v1, const float3& v2, const float3& v3, float3& normal) {
	normal = (v2 - v1).cross(v3 - v1);
	normal.normalize();
}

/*! computes the normal, binormal and tangent of a triangle
 *! specified by v1, v2 and v3 and their texture coordinates
 *  @param v1 the first vertex
 *  @param v2 the second vertex
 *  @param v3 the third vertex
 *  @param normal the "output" normal
 *  @param binormal the "output" binormal
 *  @param tangent the "output" tangent
 *  @param t1 the first texture coordinate
 *  @param t2 the second texture coordinate
 *  @param t3 the third texture coordinate
 */
void core::compute_normal_tangent_binormal(const float3& v1, const float3& v2, const float3& v3,
										   float3& normal, float3& binormal, float3& tangent,
										   const float2& t1, const float2& t2, const float2& t3) {
	// compute deltas
	const float delta_x1 = t2.x - t1.x;
	const float delta_y1 = t2.y - t1.y;
	const float delta_x2 = t3.x - t1.x;
	const float delta_y2 = t3.y - t1.y;
	
	// normal
	const float3 edge1(v2 - v1);
	const float3 edge2(v3 - v1);
	normal = edge1.crossed(edge2);
	normal.normalize();
	
	// binormal
	binormal = (edge1 * delta_x2) - (edge2 * delta_x1);
	binormal.normalize();
	
	// tangent
	tangent = (edge1 * delta_y2) - (edge2 * delta_y1);
	tangent.normalize();
	
	// adjust
	float3 txb = tangent.crossed(binormal);
	if(normal.dot(txb) > 0.0f) {
		tangent *= -1.0f;
	}
	else binormal *= -1.0f;
}

void core::system(const string& cmd) {
	string str_dump;
	core::system(cmd, str_dump);
}

void core::system(const string& cmd, string& output) {
	static constexpr size_t buffer_size = 8192;
	char buffer[buffer_size+1];
	memset(&buffer, 0, buffer_size+1);
	
#if defined(_MSC_VER)
#define popen _popen
#define pclose _pclose
#endif
	FILE* sys_pipe = popen(
#if !defined(__WINDOWS__)
						   cmd.c_str()
#else // always wrap commands in quotes, b/c #windowsproblems
						   ("\"" + cmd + "\"").c_str()
#endif
						   , "r");
	while(fgets(buffer, buffer_size, sys_pipe) != nullptr) {
		output += buffer;
		memset(buffer, 0, buffer_size); // size+1 is always 0
	}
	pclose(sys_pipe);
}

void core::set_random_seed(const unsigned int& seed) {
	gen.seed(seed);
}

string core::encode_url(const string& url) {
	stringstream result;
	for(auto citer = url.begin(); citer != url.end(); citer++) {
		switch(*citer) {
			case '!':
			case '#':
			case '$':
			case ';':
			case ':':
			case '=':
			case '?':
			case '@':
				result << *citer;
				break;
			default:
				if((*citer >= 'a' && *citer <= 'z') ||
				   (*citer >= 'A' && *citer <= 'Z') ||
				   (*citer >= '0' && *citer <= '9') ||
				   (*citer >= '&' && *citer <= '*') ||
				   (*citer >= ',' && *citer <= '/') ||
				   (*citer >= '[' && *citer <= '`') ||
				   (*citer >= '{' && *citer <= '~')) {
					result << *citer;
					break;
				}
				result << '%' << uppercase << hex << (*citer & 0xFF);
				break;
		}
	}
	return result.str();
}

static inline char bits_to_hex(const uint8_t& bits) {
	switch(bits) {
		case 0x0: return '0';
		case 0x1: return '1';
		case 0x2: return '2';
		case 0x3: return '3';
		case 0x4: return '4';
		case 0x5: return '5';
		case 0x6: return '6';
		case 0x7: return '7';
		case 0x8: return '8';
		case 0x9: return '9';
		case 0xA: return 'A';
		case 0xB: return 'B';
		case 0xC: return 'C';
		case 0xD: return 'D';
		case 0xE: return 'E';
		case 0xF: return 'F';
	}
	return '0';
}

string core::str_hex_escape(const string& str) {
	string ret = "";
	ret.reserve(str.size() * 4);
	
	for(const auto& ch : str) {
		ret.push_back('\\');
		ret.push_back('x');
		const uint8_t& uch = *(uint8_t*)&ch;
		ret.push_back(bits_to_hex((uch >> 4) & 0xF));
		ret.push_back(bits_to_hex(uch & 0xF));
	}
	
	return ret;
}

string core::cptr_hex_escape(const char* data, const size_t size) {
	string ret = "";
	const uint8_t* ch_ptr = (const uint8_t*)data;
	if(size != 0) {
		ret.reserve(size * 4);
		for(size_t i = 0; i < size; ++i, ++ch_ptr) {
			ret.push_back('\\');
			ret.push_back('x');
			ret.push_back(bits_to_hex((*ch_ptr >> 4) & 0xF));
			ret.push_back(bits_to_hex(*ch_ptr & 0xF));
		}
	}
	else {
		for(; *ch_ptr != '\0'; ++ch_ptr) {
			ret.push_back('\\');
			ret.push_back('x');
			ret.push_back(bits_to_hex((*ch_ptr >> 4) & 0xF));
			ret.push_back(bits_to_hex(*ch_ptr & 0xF));
		}
	}
	return ret;
}

uint32_t core::unix_timestamp() {
	return (uint32_t)chrono::duration_cast<chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count();
}

uint32_t core::get_hw_thread_count() {
	uint32_t hw_thread_count = 1; // default to 1
#if (defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__))
	size_t size = sizeof(hw_thread_count);
#if !defined(__OpenBSD__)
	sysctlbyname("hw.ncpu", &hw_thread_count, &size, nullptr, 0);
#else
	static const int sysctl_cmd[2] { CTL_HW, HW_NCPU };
	sysctl(sysctl_cmd, 2, &hw_thread_count, &size, nullptr, 0);
#endif
#elif defined(__linux__)
	hw_thread_count = (uint32_t)sysconf(_SC_NPROCESSORS_CONF);
#elif defined(__WINDOWS__)
	constexpr const uint32_t buffer_size { 16u };
	uint32_t size = buffer_size - 1;
	char output[buffer_size];
	GetEnvironmentVariable("NUMBER_OF_PROCESSORS", output, size);
	output[buffer_size - 1] = 0;
	hw_thread_count = stou(output);
#else // other platforms?
#endif
	return hw_thread_count;
}

void core::set_current_thread_name(const string&
#if defined(_PTHREAD_H) // suppress unused warning
								   thread_name
#endif
								   ) {
#if defined(_PTHREAD_H)
	// pthreads restricts name sizes to 15 characters (+one \0)
	const string name = (thread_name.size() > 15 ? thread_name.substr(0, 15) : thread_name);
	pthread_setname_np(
#if !defined(__APPLE__)
					   pthread_self(),
#endif
					   name.c_str());
#endif
}

string core::get_current_thread_name() {
#if defined(_PTHREAD_H)
	char thread_name[16];
	pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name));
	thread_name[15] = 0;
	return thread_name;
#else
	return "<unknown>";
#endif
}

bool core::cpu_has_fma() {
#if !defined(FLOOR_IOS)
	int eax, ebx, ecx, edx;
	__cpuid(1, eax, ebx, ecx, edx);
	return (ecx & bit_FMA) > 0;
#else
#if defined(PLATFORM_X64)
	return true; // armv8
#else
	return false; // armv7
#endif
#endif
}

string core::create_tmp_file_name(const string prefix, const string suffix) {
	seed_seq seed {
		rd(),
		(uint32_t)(uintptr_t)&prefix,
		unix_timestamp()
	};
	mt19937 twister { seed };
	twister.discard(32);
	
	static constexpr const char chars[] {
		'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
		'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
		'u', 'v', 'w', 'x', 'y', 'z',
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
	};
	uniform_int_distribution<> dist(0, size(chars) - 1);
	
	string ret {
#if !defined(_MSC_VER)
		"/tmp/" +
#endif
		prefix
	};
	
	static constexpr const int random_char_count { 16 };
	for(int i = 0; i < random_char_count; ++i) {
		ret += chars[dist(twister)];
	}
	
	ret += suffix;
	return ret;
}

string core::to_file_name(const string& str) {
	string ret = str;
	for(auto& ch : ret) {
		if((ch >= 'A' && ch <= 'Z') ||
		   (ch >= 'a' && ch <= 'z') ||
		   (ch >= '0' && ch <= '9') ||
		   (ch >= '#' && ch <= '.') ||
		   (ch >= ':' && ch <= '@') ||
		   (ch >= '{' && ch <= '~') ||
		   ch == '!' || ch == '[' ||
		   ch == ']' || ch == '^' ||
		   ch == '_' || ch == '`') {
			continue;
		}
		ch = '_';
	}
	return ret;
}
