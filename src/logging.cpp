/*
 * logging.cpp
 *
 *  Created on: Oct 3, 2016
 *      Author: nullifiedcat
 */

#include <stdarg.h>
#include <string.h>

#include <pwd.h>

#include "common.h"
#include "sdk.h"

FILE* logging::handle = 0;

void logging::Initialize() {
	// FIXME other method of naming the file?
	passwd* pwd = getpwuid(getuid());
	logging::handle = fopen(strfmt("/tmp/cathook-%s.log", pwd->pw_name), "w");
}

void logging::Info(const char* fmt, ...) {
	if (logging::handle == 0) logging::Initialize();
	char* buffer = new char[1024];
	va_list list;
	va_start(list, fmt);
	vsprintf(buffer, fmt, list);
	va_end(list);
	size_t length = strlen(buffer);
	char* result = new char[length + 9];
	sprintf(result, "[CAT] %s\n", buffer);
	fprintf(logging::handle, "%s", result);
	fflush(logging::handle);
#ifndef TEXTMODE
	if (g_ICvar) {
		if (console_logging.convar_parent && console_logging)
			g_ICvar->ConsolePrintf("%s", result);
	}
#else
	printf("%s", result);
#endif
	delete [] buffer;
	delete [] result;
}

void logging::Shutdown() {
	fclose(logging::handle);
	logging::handle = 0;
}
