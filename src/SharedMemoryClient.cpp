#include "SharedMemoryClient.h"

#include <iostream>
#include <fstream>

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/lexical_cast.hpp>

#include "MemoryManager.h"
#include "TransferChunk.h"
#include "SharedMemoryConsts.h"
#include "Logger.h"

SharedMemoryClient::SharedMemoryClient()
	: _sharedSegment(open_only, SHARED_MEMORY_NAME)
{
	_memoryManagerPtr = _sharedSegment.find<MemoryManager>(SHARED_MEMORY_MANAGER_NAME).first;
	_sharedTransferChunkArray = _sharedSegment.find<TransferChunk>(SHARED_TRANSFER_ARRAY_NAME).first;

	if(IsInited())
		_clientStatus.store(SharedMemoryClientStatus::INITED);
}

SharedMemoryClientStatus SharedMemoryClient::GetClientStatus() const
{
	return _countThreads == 0
			? SharedMemoryClientStatus::COMPLETED
			: _clientStatus.load();
}

bool SharedMemoryClient::IsInited() const
{
	return _memoryManagerPtr && _memoryManagerPtr->GetMemoryManagerStatus() == MemoryManagerStatus::READY
			&& _sharedTransferChunkArray;
}

void SharedMemoryClient::TransferFiles(const std::vector<std::string>& filePathsContainer)
{
	std::string threadIdString = boost::lexical_cast<std::string>(boost::this_thread::get_id());
	if(!IsInited())
	{
		TRACE("[%ld] [%s] SharedMemoryClient has not been inited properly\n", getMicrotime(), threadIdString.c_str());
		return;
	}

	_clientStatus.store(SharedMemoryClientStatus::TRANSFERRING);

	for(const std::string& filePath : filePathsContainer)
	{
		if(filePath.length() > MAX_FILE_NAME_LENGTH - 1)
		{
			TRACE("[%ld] [%s] File %s has length more than 255 chars and will be ignored\n", getMicrotime(), threadIdString.c_str(), filePath.c_str());
			continue;
		}
		TransferChunk* transferChunkPtr = GetTransferChunk();

		++_countThreads;
		boost::thread(boost::bind(&SharedMemoryClient::TransferThread, this, transferChunkPtr, filePath));
	}
}

TransferChunk* SharedMemoryClient::GetTransferChunk()
{
	std::string threadIdString = boost::lexical_cast<std::string>(boost::this_thread::get_id());
	uint32_t timeoutStrikeCounter = 0;
	TransferChunk* transferChunkPtr = nullptr;
	while(true)
	{
		scoped_lock<interprocess_mutex> lock(_memoryManagerPtr->_memoryManagerMutex);

		_memoryManagerPtr->SetMemoryManagerStatus(MemoryManagerStatus::REQ_TO_ALLOC_CHUNK);
		_memoryManagerPtr->_cvSignalToServer.notify_one();

		if(!_memoryManagerPtr->_cvSignalToClient.timed_wait(lock, boost::get_system_time() + boost::posix_time::milliseconds(CONDITIONAL_VARIABLE_TIMEOUT_MILLISECONDS)
													, [&] {
														return _memoryManagerPtr->GetMemoryManagerStatus() != MemoryManagerStatus::REQ_TO_ALLOC_CHUNK;
													}))
		{
			//	timeout handler
			++timeoutStrikeCounter;
			TRACE("[%ld] [%s] Waiting timeout. %d strike\n", getMicrotime(), threadIdString.c_str(), timeoutStrikeCounter);
			if(timeoutStrikeCounter == CONDITIONAL_VARIABLE_STRIKE_LIMIT)
			{
				TRACE("[%ld] [%s] Server doesn't respond. Thread will be terminated\n", getMicrotime(), threadIdString.c_str());
				break;
			}
			continue;
		}

		timeoutStrikeCounter = 0;
		if(_memoryManagerPtr->GetMemoryManagerStatus()  == MemoryManagerStatus::ALLOC_IS_SUCCESSFUL)
		{
			uint32_t transferChunkIndex = _memoryManagerPtr->GetLastChunkIndex();
			transferChunkPtr = &_sharedTransferChunkArray[transferChunkIndex];

			TRACE("[%ld] [%s] New chunk address: %p\n", getMicrotime(), threadIdString.c_str(), transferChunkPtr);
			break;
		}
		else
		{
			TRACE("[%ld] [%s] New transfer chunk allocation error\n", getMicrotime(), threadIdString.c_str());
			break;
		}
	}

	_memoryManagerPtr->SetMemoryManagerStatus(MemoryManagerStatus::REQ_TO_ALLOC_IS_COMPLETED);
	_memoryManagerPtr->_cvSignalToServer.notify_one();
	return transferChunkPtr;
}

void SharedMemoryClient::TransferThread(TransferChunk* transferChunkPtr, const std::string& filePath)
{
	if(!transferChunkPtr || filePath.empty())
	{
		--_countThreads;
		return;
	}

	std::string threadIdString = boost::lexical_cast<std::string>(boost::this_thread::get_id());
	TRACE("[%ld] [%s] SharedMemoryClient::TransferThread has been started\n", getMicrotime(), threadIdString.c_str());
	TRACE("[%ld] [%s] File %s is transmitting\n", getMicrotime(), threadIdString.c_str(), filePath.c_str());
	TRACE("[%ld] [%s] TransferChunk address: %p\n", getMicrotime(), threadIdString.c_str(), &*transferChunkPtr);

	uint32_t timeoutStrikeCounter = 0;
	try
	{
		std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);

		if (file.is_open())
		{
			scoped_lock<interprocess_mutex> lock(transferChunkPtr->_transferMutex);

			strncpy(transferChunkPtr->_fileName, filePath.c_str(), MAX_FILE_NAME_LENGTH - 1);

			while(file)
			{
				file.read(reinterpret_cast<char*>(&transferChunkPtr->_data[0]), DATA_FRAME_SIZE);
				transferChunkPtr->_countBytes = static_cast<std::uint32_t>(file.gcount());

				if (!transferChunkPtr->_countBytes)
				{
					break;
				}

				transferChunkPtr->_transferStatus = TransferChunkStatus::REQ_TO_READ;
				transferChunkPtr->_cvToRead.notify_one();

				if(!transferChunkPtr->_cvToWrite.timed_wait(lock, boost::get_system_time() + boost::posix_time::milliseconds(CONDITIONAL_VARIABLE_TIMEOUT_MILLISECONDS)
															, [&] {
																return transferChunkPtr->_transferStatus == TransferChunkStatus::REQ_TO_WRITE;
															}))
				{
					//	timeout handler
					++timeoutStrikeCounter;
					TRACE("[%ld] [%s] Waiting timeout. %d strike\n", getMicrotime(), threadIdString.c_str(), timeoutStrikeCounter);
					if(timeoutStrikeCounter == CONDITIONAL_VARIABLE_STRIKE_LIMIT)
					{
						TRACE("[%ld] [%s] Server doesn't respond. Thread will be terminated\n", getMicrotime(), threadIdString.c_str());
						break;
					}
					continue;
				}
				timeoutStrikeCounter = 0;
			}
			transferChunkPtr->_transferStatus = TransferChunkStatus::TRANSFER_IS_FINISHED;
			file.close();
			transferChunkPtr->_cvToRead.notify_one();
			++_transmittedFileCounter;
		}
		else
		{
			TRACE("[%ld] [%s] Unable to open file: %s\n", getMicrotime(), threadIdString.c_str(), filePath.c_str());
		}
	}
	catch(interprocess_exception &ex)
	{
		TRACE("[%ld] [%s] boost IPC exception: %s\n", getMicrotime(), threadIdString.c_str(), ex.what());
	}

	--_countThreads;
	TRACE("[%ld] [%s] ClientTransferThread has been finished\n", getMicrotime(), threadIdString.c_str());
}
