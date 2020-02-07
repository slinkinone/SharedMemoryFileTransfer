#include "MemoryManager.h"

#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

#include "TransferChunk.h"
#include "Logger.h"

MemoryManager::MemoryManager(managed_shared_memory& sharedMemorySegment)
	: _sharedSegment(sharedMemorySegment)
	, _memoryManagerStatus(MemoryManagerStatus::NOT_INITED)
	, _lastChunkIndex(std::numeric_limits<uint32_t>::max())
{
	try
	{
		TRACE("[%ld] [%s] Shared memory size: %d; MemoryManager Size: %d; TransferChunk size: %d; MaxChunkCount: %d\n"
			  , getMicrotime(), boost::lexical_cast<std::string>(boost::this_thread::get_id()).c_str()
			  , _sharedSegment.get_size(), sizeof(MemoryManager)
			  , sizeof(TransferChunk), _maxChunkCount);
		_transferChunkContainer = _sharedSegment.construct<TransferChunk>(SHARED_TRANSFER_ARRAY_NAME)[_maxChunkCount]();
		SetMemoryManagerStatus(MemoryManagerStatus::READY);
	}
	catch(...)
	{
		TRACE("[%ld] [%s] Chunk array allocation error\n", getMicrotime(), boost::lexical_cast<std::string>(boost::this_thread::get_id()).c_str());
	}
}

void MemoryManager::SetMemoryManagerStatus(MemoryManagerStatus status)
{
	_memoryManagerStatus = status;
}

MemoryManagerStatus MemoryManager::GetMemoryManagerStatus() const
{
	return _memoryManagerStatus;
}

uint32_t MemoryManager::GetLastChunkIndex() const
{
	return _lastChunkIndex;
}

TransferChunk* MemoryManager::GetNextFreeTransferChunkPointer()
{
	uint32_t chunkIndex = (_lastChunkIndex + 1) % _maxChunkCount;
	do
	{
		if(!_transferChunkContainer[chunkIndex]._isChunkBusy)
		{
			_transferChunkContainer[chunkIndex]._isChunkBusy = true;
			_lastChunkIndex = chunkIndex;
			return &_transferChunkContainer[chunkIndex];
		}
		++chunkIndex;
		chunkIndex = chunkIndex % _maxChunkCount;
	}
	while(chunkIndex != _lastChunkIndex);

	return nullptr;
}
