#include "Logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

using namespace boost::interprocess;

void trace(const char *fmt, ...)
{
	static interprocess_mutex traceMutex;
	scoped_lock<interprocess_mutex> lock(traceMutex);

	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	fflush(stdout);
}
