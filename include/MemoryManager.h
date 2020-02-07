#pragma once

#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>

#include "TransferChunk.h"

using namespace boost::interprocess;

enum class MemoryManagerStatus : uint8_t
{
	NOT_INITED,
	READY,
	REQ_TO_ALLOC_CHUNK,
	REQ_TO_ALLOC_IS_COMPLETED,
	ALLOC_IS_SUCCESSFUL,
	ALLOCATION_ERROR,
};

class MemoryManager
{
public:
	MemoryManager() = delete;
	MemoryManager(const MemoryManager&) = delete;
	MemoryManager(MemoryManager&) = delete;
	MemoryManager(MemoryManager&&) = delete;

public:
	//MemoryManager (allocator_type customAllocator, uint32_t chunkCount);
	MemoryManager(managed_shared_memory& sharedMemorySegment);

public:
	void SetMemoryManagerStatus(MemoryManagerStatus status);
	MemoryManagerStatus GetMemoryManagerStatus() const;
	uint32_t GetLastChunkIndex() const;
	TransferChunk* GetNextFreeTransferChunkPointer();

public:
	interprocess_mutex _memoryManagerMutex;
	interprocess_condition _cvSignalToServer;
	interprocess_condition _cvSignalToClient;

private:
	managed_shared_memory& _sharedSegment;
	TransferChunk* _transferChunkContainer;

private:
	MemoryManagerStatus _memoryManagerStatus;
	uint32_t _lastChunkIndex;
	const uint32_t _maxChunkCount = (VIRTUAL_SHARED_MEMORY_SIZE - sizeof(MemoryManager))
										/ sizeof(TransferChunk) - 1;
};
