/*
 * Copyright (c) 2015-2017, Parallels International GmbH
 *
 * This file is part of OpenVZ. OpenVZ is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * Lesser General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Our contact details: Parallels International GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 *
 * common functions of tool
 */

#include "common.h"

#define PROG_NAME	"prl_nettool"

#include <stdarg.h>
#include <time.h>
#include <stdio.h>

#ifdef _WIN_

#include <windows.h>

#define LOGFILE	"C:\\prl_nettool.log"

#ifdef vsnprintf
#undef vsnprintf
#endif
#define vsnprintf _vsnprintf
#define getpid	GetCurrentProcessId
#define CRLF	"\r\n"

void error(int err, const char *fmt, ...)
{
	LPVOID lpMsgBuf;
	char *errorMessage;
	char errorString[ 30 ];
	char msg[1024];
	va_list args;

	if (fmt) {
		va_start(args, fmt);
		vsnprintf(msg, 1023, fmt, args);
		va_end(args);
		msg[1023]='\0';
	} else {
		msg[0] = '\0';
	}

	fprintf(stderr, "%s", msg);

	debug("error %s", msg);

	if (err != 0) {
		if( FormatMessageA( FORMAT_MESSAGE_ALLOCATE_BUFFER |
					FORMAT_MESSAGE_FROM_SYSTEM |
					FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				err,
				0, // Default language
				(LPSTR) &lpMsgBuf,
				0,
				NULL ) == 0 )
		{
			sprintf_s( errorString, sizeof(errorString), "Unknown Error (%d)", err );
				errorMessage = errorString;
				lpMsgBuf = NULL;
		} else {
			errorMessage = lpMsgBuf;
		}

		fprintf(stderr, ": %s", errorMessage);
		debug("error %d: %s", err, errorMessage);

		LocalFree( lpMsgBuf );
	}

	fprintf(stderr, "\n");
	fflush(stderr);
}



int is_ipv6_supported()
{
	DWORD dwVersion = 0;
	DWORD dwMajorVersion = 0;
	DWORD dwMinorVersion = 0;

	dwVersion = GetVersion();

	dwMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)));
	dwMinorVersion = (DWORD)(HIBYTE(LOWORD(dwVersion)));
	(void)dwMinorVersion;

	if (dwMajorVersion >= 6)
		return 1;

	return 0;
}

#else

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define LOGFILE	"/var/log/prl_nettool.log"

#define CRLF	"\n"

void error(int err, const char *fmt, ...)
{
	va_list args;
	char log_msg[1024];

	if (fmt) {
		va_start(args, fmt);
		vsnprintf(log_msg, 1023, fmt, args);
		va_end(args);
		log_msg[1023]='\0';
	} else {
		log_msg[0] = '\0';
	}

	fprintf(stderr, "%s: %s", PROG_NAME, log_msg);

	if (err != 0)
		fprintf(stderr, ": %s", strerror(err));

	debug("error %s: %s", PROG_NAME, log_msg);
	if (err != 0)
		debug("error %d: %s", err, strerror(err));

	fprintf(stderr, "\n");
	fflush(stderr);
}

int is_ipv6_supported()
{
	//TODO: implement detection on linux/mac
	return 1;
}

#endif

void debug(const char *fmt, ...)
{
	time_t t = time(NULL);
	struct tm * tm = localtime(&t);
	char * strtime = asctime(tm);
	char msg[1024];
	size_t len = 0;
	int pid = (int)getpid();
#ifdef _WIN_
	DWORD dw;
	HANDLE fd = CreateFileA(LOGFILE, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fd == INVALID_HANDLE_VALUE)
		fd = NULL;
#else
	int fd = open(LOGFILE, O_WRONLY|O_APPEND);
	if (fd < 0)
		fd = 0;
#endif

	if (fd) {
		len += snprintf(msg + len, sizeof(msg) - len, "%s %d ", strtime, pid);
		if (fmt) {
			va_list args;
			va_start(args, fmt);
			len += vsnprintf(msg + len, sizeof(msg) - len, fmt, args);
			va_end(args);
		}
		len += snprintf(msg + len, sizeof(msg) - len, CRLF);
		msg[sizeof(msg)-1] = 0;
#ifdef _WIN_
		WriteFile(fd, msg, strlen(msg), &dw, NULL);
		CloseHandle(fd);
#else
		write(fd, msg, strlen(msg));
		close(fd);
#endif
	}
}
