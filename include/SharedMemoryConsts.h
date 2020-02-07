#pragma once

#include <cstdint>
#include <string>

constexpr uint32_t MAIN_THREAD_SLEEP_TIME = 3;
constexpr uint32_t CONDITIONAL_VARIABLE_TIMEOUT_MILLISECONDS = 3000;
constexpr uint32_t CONDITIONAL_VARIABLE_STRIKE_LIMIT = 3;
constexpr uint32_t VIRTUAL_SHARED_MEMORY_SIZE = 256*1024*1024;		//	shares memory size in bytes (256KB)
constexpr uint32_t DATA_FRAME_SIZE = 256;
constexpr uint32_t MAX_FILE_NAME_LENGTH = 256;
constexpr char SHARED_MEMORY_NAME[] = "SEARCH_INFORM_FILE_TRANSFER_SHARED_MEMORY";
constexpr char SHARED_MEMORY_MANAGER_NAME[] = "FILE_TRANSFER_MEMORY_MANAGER";
constexpr char SHARED_TRANSFER_ARRAY_NAME[] = "FILE_TRANSFER_CHUNK_ARRAY";

extern long getMicrotime();
