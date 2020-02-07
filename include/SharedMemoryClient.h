#pragma once

#include <string>
#include <vector>

#include <boost/thread.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>

using namespace boost::interprocess;

class TransferChunk;
class MemoryManager;

enum class SharedMemoryClientStatus : uint8_t
{
	NOT_INITED,
	INITED,
	TRANSFERRING,
	COMPLETED,
};

class SharedMemoryClient
{
public:
	SharedMemoryClient();
	SharedMemoryClient(const SharedMemoryClient&) = delete;
	SharedMemoryClient(SharedMemoryClient&) = delete;
	SharedMemoryClient(SharedMemoryClient&&) = delete;

public:
	SharedMemoryClientStatus GetClientStatus() const;
	void TransferFiles(const std::vector<std::string>& filePathsContainer);

private:
	bool IsInited() const;
	TransferChunk* GetTransferChunk();
	void TransferThread(TransferChunk* transferChunk, const std::string& filePath);

private:
	managed_shared_memory _sharedSegment;

private:
	std::atomic<SharedMemoryClientStatus> _clientStatus = {SharedMemoryClientStatus::NOT_INITED};
	std::atomic<uint32_t> _countThreads = {0};
	std::atomic<uint32_t> _transmittedFileCounter = {0};
	MemoryManager* _memoryManagerPtr;
	TransferChunk* _sharedTransferChunkArray;
};
