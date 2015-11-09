/*
 * Copyright (c) 2015 Parallels IP Holdings GmbH
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
 * Our contact details: Parallels IP Holdings GmbH, Vordergasse 59, 8200
 * Schaffhausen, Switzerland.
 *
 * methods for executing external commands
 */

#include "../common.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <syslog.h>

#include "exec.h"

int run_cmd(const char *cmd)
{
	int ret, status;

	/* for debug
	openlog("prl_nettool", LOG_PID | LOG_CONS, LOG_USER);
	syslog(LOG_ERR | LOG_USER, "run: %s", cmd);
	closelog();
	werror("run: %s", cmd);
	*/

	status = system(cmd);

	if (status == -1){
		error(0, "Failed to execute: %s", cmd);
		ret = -1;
	} else if (WIFEXITED(status)) {
		ret = WEXITSTATUS(status);
		//werror("execCmd: %s [%d]\n", cmd, ret);
	} else if (WIFSIGNALED(status)) {
		werror("command %s got signal %d\n", cmd, WTERMSIG(status));
		ret = -1;
	} else {
		werror("run cmd: %s\n", cmd);
		return -1;
	}
	return ret;
}

