#include <cstring>
#include <cstdlib>
#include <ctime>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

#include <iostream>
#include <vector>
#include <fstream>

#include "MemoryManager.h"
#include "TransferChunk.h"
#include "SharedMemoryClient.h"
#include "SharedMemoryServer.h"
#include "SharedMemoryConsts.h"
#include "Logger.h"

int main(int argc, char *argv[])
{
	std::string threadIdString = boost::lexical_cast<std::string>(boost::this_thread::get_id());
	if(argc < 2)
	{
		TRACE("[%ld] [%s] The application takes at least 2 params: client/server\n", getMicrotime(), threadIdString.c_str());
		return 0;
	}

	if(strcmp(argv[1], "client") == 0)
	{
		std::vector<std::string> filePathsContainer;
		for(int i = 2; i < argc; ++i)
			filePathsContainer.emplace_back(argv[i]);

		if(filePathsContainer.size() == 0)
		{
			TRACE("[%ld] [%s] The client application takes file paths for transmitting to server\n", getMicrotime(), threadIdString.c_str());
			return 0;
		}

		TRACE("[%ld] [%s] Shared memory client\n", getMicrotime(), threadIdString.c_str());

		SharedMemoryClient sharedMemoryClient;
		sharedMemoryClient.TransferFiles(filePathsContainer);

		while (sharedMemoryClient.GetClientStatus() == SharedMemoryClientStatus::TRANSFERRING)
		{
			sleep(MAIN_THREAD_SLEEP_TIME);
		}
	}
	else if(strcmp(argv[1], "server") == 0)
	{
		TRACE("[%ld] [%s] Shared memory server\n", getMicrotime(), threadIdString.c_str());

		SharedMemoryServer sharedMemoryServer;
		sharedMemoryServer.Start();

		while(sharedMemoryServer.GetServerStatus() == SharedMemoryServerStatus::RUNNING)
		{
			sleep(MAIN_THREAD_SLEEP_TIME);
		}
	}
	else
	{
		TRACE("[%ld] [%s] The first paramenter isn't recognized. It should be 'client' or 'server'\n", getMicrotime(), threadIdString.c_str());
		return 0;
	}

	return 0;
}
