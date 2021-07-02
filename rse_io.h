#pragma once
#ifdef _WIN32
#include <Windows.h>
#elif __linux__
#include <sys/mman.h>
#endif

#include "rse_ds.h"

namespace rse {

	namespace io {

		enum class MemMapIO {
			READ_WRITE,
			READ_ONLY
		};

		// Attempts top open a file and read it's content into an allocated 
		// buffer with malloc. It's up to you to free this.
		char* AllocateIntoBuffer(const char* filename, size_t &out_size) {
			FILE *file = fopen("test.txt", "rb");
			if (file == NULL) {
				printf("Failed to open test file!");
				return nullptr;
			}
			fseek(file, 0, SEEK_END);
			out_size = ftell(file);
			fseek(file, 0, SEEK_SET);  /* same as rewind(f); */

			char* buffer = (char*)malloc(out_size);
			if (buffer == nullptr) return nullptr;
			fread(buffer, 1, out_size, file);
			fclose(file);

			return buffer;
		}

		// This is mostly just for windows
		struct MemMap {
			void* ptr = nullptr;
			#ifdef _WIN32
			HANDLE h_file = 0;
			HANDLE h_mapping_obj = 0;
			#endif
			uint64_t num_bytes = 0;
		};

		// Does nothing if the pointer is null
		void UnmapMemory(const MemMap& m) {
			if (m.ptr == nullptr) return;


			#ifdef _WIN32
			FlushViewOfFile(
				m.ptr,
				m.num_bytes
			);
			UnmapViewOfFile(m.ptr);
			CloseHandle(m.h_mapping_obj);
			CloseHandle(m.h_file);
			#else 
			
			munmap(m.ptr, m.num_bytes);
			#endif
		}

		// Because windows is stupid we need an api
		// to do file mapping that is cross platform
		// This is not thread safe
		bool MapMemory(const char* filename, uint64_t size, MemMapIO io, MemMap &m) {
	

			if (size == 0) return false;
#ifdef _WIN32
			DWORD file_io;
			DWORD file_map_io;
			DWORD map_view_io;
			DWORD access_type;

			file_io = GENERIC_READ | GENERIC_WRITE;
			file_map_io = PAGE_READWRITE;
			access_type = CREATE_ALWAYS;

			switch (io) {
			case MemMapIO::READ_ONLY:
				map_view_io = FILE_MAP_READ;
				access_type = OPEN_ALWAYS;
				break;
			default:
				map_view_io = FILE_MAP_ALL_ACCESS;
				break;
			}

			m.num_bytes = size;
			m.h_file = CreateFileA(
				filename,
				file_io,
				0,
				NULL,
				access_type,
				FILE_ATTRIBUTE_NORMAL,
				NULL
			);
			if (m.h_file == INVALID_HANDLE_VALUE) return false;

			m.h_mapping_obj = CreateFileMappingA(
				m.h_file,
				NULL,
				file_map_io,
				size >> 32,
				size,
				NULL
			);
			if (GetLastError() != 0 || m.h_mapping_obj == NULL) {

				CloseHandle(m.h_file);
				return false;
			}
			m.ptr = MapViewOfFile(
				m.h_mapping_obj,
				map_view_io,
				0, 
				0,
				size
			);
			if (m.ptr == NULL) {
				CloseHandle(m.h_mapping_obj);
				CloseHandle(m.h_file);
				return false;
			}
#elif __linux__
			int fd = -1;
			int prot = 0;
			switch (io) {
				case MemMapIO::READ_ONLY:
					prot = PROT_READ;
					fd = open(filename, O_CREAT | O_RDONLY);
					break;
				case MemMapIO::READ_WRITE:
					prot = PROT_READ | PROT_WRITE;
					fd = open(filename, O_CREAT | O_RDWR);
					break;
			}

			if (fd == -1) {
				debug_printf("Failed to open file [%s]\n", strerror(errno));
				return false;
			}

			char *ptr = (char*)mmap(nullptr, size, prot, MAP_SHARED, fd, 0);	
			close(fd);
			if (ptr == MAP_FAILED) {
				debug_printf("Failed to map file [%s]\n", strerror(errno));
				return false;
			}

			m.ptr = ptr;
			m.num_bytes = size;
#endif 

			return true;
		}


		static bool TestMemMap() {

			const size_t SIZE = 64;
			static char buffer[SIZE];

			MemMap mem_map;
			if (!MapMemory("mem_map_test.txt", 64, MemMapIO::READ_WRITE, mem_map)) {
				printf("Failed to mem map file\n");
				return false;
			}

			memset(buffer, 'a', 64);
			memcpy(mem_map.ptr, buffer, 64);

			UnmapMemory(mem_map);

			return true;
		}

	}


}