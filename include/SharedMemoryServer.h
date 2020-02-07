#pragma once

#include <vector>
#include <memory>
#include <boost/thread.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>

#include "SharedMemoryConsts.h"

using namespace boost::interprocess;

class MemoryManager;
class TransferChunk;

enum class SharedMemoryServerStatus : uint8_t
{
	NOT_INITED,
	INITED,
	RUNNING,
	STOPPED,
};

struct SharedMemoryCleaner
{
	SharedMemoryCleaner();
	~SharedMemoryCleaner();
};

class SharedMemoryServer
{
public:
	SharedMemoryServer();
	SharedMemoryServer(const SharedMemoryServer&) = delete;
	SharedMemoryServer(SharedMemoryServer&) = delete;
	SharedMemoryServer(SharedMemoryServer&&) = delete;

public:
	SharedMemoryServerStatus GetServerStatus() const;
	void Start();
	void Stop();

private:
	bool IsInited() const;
	void MemoryManagerThread();
	void TransferThread(TransferChunk* transferChunk);

private:
	SharedMemoryCleaner _sharedMemoryCleaner;
	managed_shared_memory _sharedSegment;

private:
	std::atomic<SharedMemoryServerStatus> _serverStatus = {SharedMemoryServerStatus::NOT_INITED};
	MemoryManager* _memoryManagerPtr = nullptr;
	std::atomic<uint32_t> _countTransferThreads = {0};
};
