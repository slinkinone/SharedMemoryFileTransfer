#pragma once

#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
//#include <boost/interprocess/smart_ptr/unique_ptr.hpp>

#include "SharedMemoryConsts.h"

using namespace boost::interprocess;

enum class TransferChunkStatus : uint8_t
{
	NOT_INITED,
	REQ_TO_READ,
	REQ_TO_WRITE,
	TRANSFER_IS_FINISHED,
};

struct TransferChunk
{
	TransferChunk ();
	bool _isChunkBusy = false;
	interprocess_mutex _transferMutex;
	interprocess_condition _cvToRead;
	interprocess_condition _cvToWrite;

	TransferChunkStatus _transferStatus = TransferChunkStatus::NOT_INITED;
	char _fileName[MAX_FILE_NAME_LENGTH] = {0};
	uint8_t _data[DATA_FRAME_SIZE] = {0};
	uint32_t _countBytes = 0;
};
