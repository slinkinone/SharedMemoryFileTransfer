#include "SharedMemoryServer.h"

#include <iostream>
#include <fstream>

#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

#include "MemoryManager.h"
#include "TransferChunk.h"
#include "Logger.h"

SharedMemoryCleaner::SharedMemoryCleaner()
{
	shared_memory_object::remove(SHARED_MEMORY_NAME);
}

SharedMemoryCleaner::~SharedMemoryCleaner()
{
	shared_memory_object::remove(SHARED_MEMORY_NAME);
}

SharedMemoryServer::SharedMemoryServer()
	: _sharedSegment(create_only, SHARED_MEMORY_NAME, VIRTUAL_SHARED_MEMORY_SIZE)
{
	try
	{
		_memoryManagerPtr = _sharedSegment.construct<MemoryManager>(SHARED_MEMORY_MANAGER_NAME)(_sharedSegment);
	}
	catch(...)
	{
		TRACE("[%ld] [%s] MemoryManager creating error\n", getMicrotime(), boost::lexical_cast<std::string>(boost::this_thread::get_id()).c_str());
		return;
	}

	if(_memoryManagerPtr &&
		_memoryManagerPtr->GetMemoryManagerStatus() == MemoryManagerStatus::READY)
		_serverStatus.store(SharedMemoryServerStatus::INITED);
}

bool SharedMemoryServer::IsInited() const
{
	return _memoryManagerPtr && _memoryManagerPtr->GetMemoryManagerStatus() == MemoryManagerStatus::READY;
}

void SharedMemoryServer::Start()
{
	if(_serverStatus.load() != SharedMemoryServerStatus::INITED)
		return;

	boost::thread(boost::bind(&SharedMemoryServer::MemoryManagerThread, this));
	_serverStatus.store(SharedMemoryServerStatus::RUNNING);
}

void SharedMemoryServer::Stop()
{
	//	stub method
	_serverStatus.store(SharedMemoryServerStatus::STOPPED);
}

SharedMemoryServerStatus SharedMemoryServer::GetServerStatus() const
{
	return _serverStatus.load();
}

void SharedMemoryServer::MemoryManagerThread()
{
	if(!IsInited())
	{
		_serverStatus.store(SharedMemoryServerStatus::NOT_INITED);
		return;
	}

	std::string threadIdString = boost::lexical_cast<std::string>(boost::this_thread::get_id());
	TRACE("[%ld] [%s] SharedMemoryServer::MemoryManagerThread has been started\n", getMicrotime(), threadIdString.c_str(), threadIdString.c_str());

	uint32_t timeoutStrikeCounter = 0;
	TransferChunk* transferChunkPtr = nullptr;
	while(true)
	{
		scoped_lock<interprocess_mutex> lock(_memoryManagerPtr->_memoryManagerMutex);
		_memoryManagerPtr->_cvSignalToClient.notify_one();
		_memoryManagerPtr->_cvSignalToServer.timed_wait(lock, boost::get_system_time() + boost::posix_time::milliseconds(CONDITIONAL_VARIABLE_TIMEOUT_MILLISECONDS)
													, [&] {
															return _memoryManagerPtr->GetMemoryManagerStatus() == MemoryManagerStatus::REQ_TO_ALLOC_CHUNK
															|| _memoryManagerPtr->GetMemoryManagerStatus() == MemoryManagerStatus::REQ_TO_ALLOC_IS_COMPLETED;
													});

		if(_memoryManagerPtr->GetMemoryManagerStatus() == MemoryManagerStatus::REQ_TO_ALLOC_CHUNK)
		{
			timeoutStrikeCounter = 0;
			transferChunkPtr = _memoryManagerPtr->GetNextFreeTransferChunkPointer();
			if(transferChunkPtr != nullptr)
			{
				TRACE("[%ld] [%s] Allocated new chunk address: %p\n", getMicrotime(), threadIdString.c_str(), &*transferChunkPtr);
				_memoryManagerPtr->SetMemoryManagerStatus(MemoryManagerStatus::ALLOC_IS_SUCCESSFUL);
				++_countTransferThreads;
				boost::thread(boost::bind(&SharedMemoryServer::TransferThread, this, transferChunkPtr));
			}
			else
			{
				_memoryManagerPtr->SetMemoryManagerStatus(MemoryManagerStatus::ALLOCATION_ERROR);
			}
		}
		else if (_memoryManagerPtr->GetMemoryManagerStatus() == MemoryManagerStatus::ALLOC_IS_SUCCESSFUL
				 || _memoryManagerPtr->GetMemoryManagerStatus() == MemoryManagerStatus::ALLOCATION_ERROR)		//	client doesn't process (status doesn't change)
			{
				++timeoutStrikeCounter;
				TRACE("[%ld] [%s] Waiting timeout. %d strike\n", getMicrotime(), threadIdString.c_str(), timeoutStrikeCounter);

				if(timeoutStrikeCounter == CONDITIONAL_VARIABLE_STRIKE_LIMIT)
				{
					TRACE("[%ld] [%s] Client doesn't respond. Thread will be terminated\n", getMicrotime(), threadIdString.c_str());
					if(transferChunkPtr)
					{
						transferChunkPtr->_transferStatus = TransferChunkStatus::NOT_INITED;
						transferChunkPtr->_isChunkBusy = false;
					}
					_memoryManagerPtr->SetMemoryManagerStatus(MemoryManagerStatus::READY);
					timeoutStrikeCounter = 0;
				}
			}
		else if (_memoryManagerPtr->GetMemoryManagerStatus() == MemoryManagerStatus::REQ_TO_ALLOC_IS_COMPLETED)
		{
				_memoryManagerPtr->SetMemoryManagerStatus(MemoryManagerStatus::READY);
				timeoutStrikeCounter = 0;
		}
	}
}

void SharedMemoryServer::TransferThread(TransferChunk* transferChunk)
{
	if(!transferChunk)
	{
		--_countTransferThreads;
		return;
	}

	std::string threadIdString = boost::lexical_cast<std::string>(boost::this_thread::get_id());
	TRACE("[%ld] [%s] ServerTransferThread has been started\n", getMicrotime(), threadIdString.c_str());
	TRACE("[%ld] [%s] TransferChunk address: %p\n", getMicrotime(), threadIdString.c_str(), &(*transferChunk));

	try
	{
		std::ofstream file(threadIdString.c_str(), std::ios::binary);
		scoped_lock<interprocess_mutex> lock(transferChunk->_transferMutex);

		uint32_t timeoutStrikeCounter = 0;
		while (true)
		{
			if(transferChunk->_transferStatus == TransferChunkStatus::REQ_TO_READ)
			{
				file.write(reinterpret_cast<char*>( transferChunk->_data ), transferChunk->_countBytes);
				if(!file.good())
					break;
			}
			else if (transferChunk->_transferStatus == TransferChunkStatus::TRANSFER_IS_FINISHED)
			{
				break;
			}

			transferChunk->_transferStatus = TransferChunkStatus::REQ_TO_WRITE;
			transferChunk->_cvToWrite.notify_one();

			if(!transferChunk->_cvToRead.timed_wait(lock, boost::get_system_time() + boost::posix_time::milliseconds(CONDITIONAL_VARIABLE_TIMEOUT_MILLISECONDS)
														, [&] {
																return transferChunk->_transferStatus == TransferChunkStatus::TRANSFER_IS_FINISHED
																|| transferChunk->_transferStatus == TransferChunkStatus::REQ_TO_READ;
														}))
			{
				//	timeout handler
				++timeoutStrikeCounter;
				TRACE("[%ld] [%s] Waiting timeout. %d strike\n", getMicrotime(), threadIdString.c_str(), timeoutStrikeCounter);
				if(timeoutStrikeCounter == CONDITIONAL_VARIABLE_STRIKE_LIMIT)
				{
					TRACE("[%ld] [%s] Client doesn't respond. Thread will be terminated\n", getMicrotime(), threadIdString.c_str());
					break;
				}
				continue;
			}
			timeoutStrikeCounter = 0;
		}

		if(timeoutStrikeCounter != CONDITIONAL_VARIABLE_STRIKE_LIMIT)
		{
			if(!file.good())
			{
				TRACE("[%ld] [%s] File writing error: %s\n", getMicrotime(), threadIdString.c_str(), threadIdString.c_str());
				file.close();
				std::remove(threadIdString.c_str());		//	NOTE: Processing of deleting errors; If it is matter.
			}
			else
			{
				std::string newFileName = std::to_string(std::time(nullptr))
											+ "_" + threadIdString + "_" + transferChunk->_fileName;

				if(std::rename(threadIdString.c_str() , newFileName.c_str()) != 0)
				{
					TRACE("[%ld] [%s] The file has been saved as: %s\n", getMicrotime(), threadIdString.c_str(), threadIdString.c_str());
				}
				else
				{
					TRACE("[%ld] [%s] The file has been saved as: %s\n", getMicrotime(), threadIdString.c_str(), newFileName.c_str());
				}
			}
		}
		else
		{
			std::remove(threadIdString.c_str());
		}

		transferChunk->_transferStatus = TransferChunkStatus::NOT_INITED;
		transferChunk->_isChunkBusy = false;
	}
	catch(interprocess_exception &ex)
	{
		TRACE("[%ld] [%s] boost IPC exception: %s\n", getMicrotime(), threadIdString.c_str(), ex.what());
	}

	--_countTransferThreads;
	TRACE("[%ld] [%s] ServerTransferThread [%s] has been finished\n", getMicrotime(), threadIdString.c_str(), threadIdString.c_str());
}
