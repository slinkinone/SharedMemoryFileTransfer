#include "SharedMemoryConsts.h"
#include <sys/time.h>						//	TODO: remove

long getMicrotime()
{
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}
