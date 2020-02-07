#pragma once
/*
	This file is used for redirection the application output
	It can be helpful when is requred switch output to file/socket/terminal/etc.
*/

#ifdef STDOUT_TRACE
extern void trace(const char *fmt, ...);
#endif

#ifdef STDOUT_DEBUG_TRACE
	//	dedebug tracer stub (__FILE__, __LINE__ options)
#endif

#ifdef FILE_TRACE
	//	file log tracer stub
#endif

#define TRACE(fmt, ...) trace(fmt, __VA_ARGS__)
