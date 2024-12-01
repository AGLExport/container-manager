/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cmcontrol.c
 * @brief	container manager command line interface
 */
#include "container-manager-interface.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

static struct option long_options[] = {
	{"help", no_argument, 0, 1},
	{"get-guest-list", no_argument, NULL, 10},
	{"get-guest-list-json", no_argument, NULL, 11},
	{"shutdown-guest-name", required_argument, NULL, 20},
	{"shutdown-guest-role", required_argument, NULL, 21},
	{"reboot-guest-name", required_argument, NULL, 22},
	{"reboot-guest-role", required_argument, NULL, 23},
	{"force-reboot-guest-name", required_argument, NULL, 24},
	{"force-reboot-guest-role", required_argument, NULL, 25},
	{"change-active-guest-name", required_argument, NULL, 30},
	{"test-trigger", required_argument, NULL, 90},
	{0, 0, 0, 0},
};

static char *status_string[] = {
	"disable",
	"not started",
	"started",
	"reboot",
	"shutdown",
	"dead",
	"exit"
};

static void usage(void)
{
	(void) fprintf(stdout,
		"usage: [options] \n\n"
	    " --help                   print help strings.\n"
	    " --get-guest-list         get guest container list from container manager.\n"
		" --get-guest-list-json    get guest container list from container manager by json.\n"
	    " --shutdown-guest-name=N  shutdown request to container manager. (N=guest name)\n"
	    " --shutdown-guest-role=R  shutdown request to container manager. (R=guest role)\n"
	    " --reboot-guest-name=N    reboot request to container manager. (N=guest name)\n"
	    " --reboot-guest-role=R    shutdown request to container manager. (R=guest role)\n"
	    " --force-reboot-guest-name=N    reboot request to container manager. (N=guest name)\n"
	    " --force-reboot-guest-role=R    shutdown request to container manager. (R=guest role)\n"
		" --change-active-guest-name=N    change active guest request to container manager. (N=guest name)\n"
	    " --test-trigger=n          Trigger test. (n=number of test.)\n"
	);
}

static int cm_socket_setup(void)
{
	struct sockaddr_un name;
	int fd = -1;
	int ret = -1;

	(void) memset(&name, 0, sizeof(name));

	// Create client socket
	fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, AF_UNIX);
	if (fd < 0) {
		goto err_return;
	}

	name.sun_family = AF_UNIX;
	(void) memcpy(name.sun_path, CONTAINER_MANAGER_EXTERNAL_SOCKET_NAME, sizeof(CONTAINER_MANAGER_EXTERNAL_SOCKET_NAME));

	ret = connect(fd, (const struct sockaddr *) &name, (sizeof(CONTAINER_MANAGER_EXTERNAL_SOCKET_NAME) + sizeof(sa_family_t)));
	if (ret < 0) {
		goto err_return;
	} // TODO EALREADY and EINTR

	return fd;

err_return:
	if (fd != -1) {
		(void) close(fd);
	}

	return -1;
}

static int cm_socket_wait_response(int fd, int timeout)
{
	int result = -1;
	int ret = -1;
	struct pollfd poll_fds[1];

	(void) memset(&poll_fds, 0, sizeof(poll_fds));

	poll_fds[0].fd = fd;
	poll_fds[0].events = (POLLIN | POLLHUP | POLLERR | POLLNVAL);

	do {
		ret = poll(poll_fds, 1, timeout);
		if (ret < 0) {
			if (errno == EINTR) {
				continue;
			}

			result = -1;
			goto return_function;
		} else if (ret == 0) {
			// Timeout
			result = -1;
			break;
		} else {
			;	//nop
		}

		if ((poll_fds[0].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) != 0) {
			result = 0;
			break;
		}

	} while(1);

return_function:

	return result;
}

void cm_get_guest_list(int json)
{
	int fd = -1;
	int ret = -1;
	ssize_t sret = -1;
	container_extif_command_get_t packet;
	container_extif_command_get_response_t response;

	(void) memset(&packet, 0, sizeof(packet));
	(void) memset(&response, 0, sizeof(response));

	// Create client socket
	fd = cm_socket_setup();
	if (fd < 0) {
		(void) fprintf(stderr,"Container manager is busy.\n");
		goto error_return;
	}

	packet.header.command = CONTAINER_EXTIF_COMMAND_GETGUESTS;
	sret = write(fd, &packet, sizeof(packet));
	if (sret < (ssize_t)sizeof(packet)) {
		(void) fprintf(stderr,"Container manager is confuse.\n");
		goto error_return;
	}

	ret = cm_socket_wait_response(fd, 1000);
	if (ret < 0) {
		(void) fprintf(stderr,"Container manager communication is un available.\n");
		goto error_return;
	}

	sret = read(fd, &response, sizeof(response));
	if (sret < (ssize_t)sizeof(response)) {
		(void) fprintf(stderr,"Container manager is confuse. sret = %ld errno = %d\n", sret, errno);
		goto error_return;
	}

	if (response.header.command == CONTAINER_EXTIF_COMMAND_RESPONSE_GETGUESTS) {
		if (json == 1) {
			int i = 0;
			(void) fprintf(stdout, "{\n");
			(void) fprintf(stdout, "	\"guest-status\": [\n");
			while (1) {
				if (response.guests[i].status >= CONTAINER_EXTIF_GUEST_STATUS_DISABLE
					&& response.guests[i].status <= CONTAINER_EXTIF_GUEST_STATUS_EXIT) {
					(void) fprintf(stdout, "		{\n");
					(void) fprintf(stdout, "			\"guest-name\": \"%s\",\n", response.guests[i].guest_name);
					(void) fprintf(stdout, "			\"role-name\": \"%s\",\n", response.guests[i].role_name);
					(void) fprintf(stdout, "			\"status\": \"%s\"\n", status_string[response.guests[i].status]);
				}

				i++;
				if (i < response.num_of_guests) {
					(void) fprintf(stdout, "		},\n");
				} else {
					(void) fprintf(stdout, "		}\n");
					break;
				}
			}
			(void) fprintf(stdout, "	]\n");
			(void) fprintf(stdout, "}\n");
		} else {
			(void) fprintf(stdout, "HEADER: %32s,%12s,%12s \n", "name", "role", "status");
			for (int i = 0; i < response.num_of_guests; i++) {
				if (response.guests[i].status >= CONTAINER_EXTIF_GUEST_STATUS_DISABLE
					&& response.guests[i].status <= CONTAINER_EXTIF_GUEST_STATUS_EXIT) {

					(void) fprintf(stdout, "        %32s,%12s,%12s \n"
						, response.guests[i].guest_name
						, response.guests[i].role_name
						, status_string[response.guests[i].status] );
				}
			}
		}
	}

error_return:
	if (fd != -1) {
		(void) close(fd);
	}

	return;
}

const char *cm_control_lifecycle_messages[] = {
	"Success to shutdown guest: name = %s\n",
	"Success to shutdown guest: role = %s\n",
	"Success to reboot guest: name = %s\n",
	"Success to reboot guest: role = %s\n",
	"Success to force reboot guest: name = %s\n",
	"Success to force reboot guest: role = %s\n",
};
void cm_get_guest_lifecycle(int code, char *name)
{
	int fd = -1;
	int ret = -1;
	int message_index = 0;
	ssize_t sret = -1;
	container_extif_command_lifecycle_t packet;
	container_extif_command_lifecycle_response_t response;

	(void) memset(&packet, 0, sizeof(packet));
	(void) memset(&response, 0, sizeof(response));

	// Create client socket
	fd = cm_socket_setup();
	if (fd < 0) {
		(void) fprintf(stderr,"Container manager is busy.\n");
		goto error_return;
	}

	if (code == 20) {
		packet.header.command = CONTAINER_EXTIF_COMMAND_LIFECYCLE_GUEST_NAME;
		packet.subcommand = CONTAINER_EXTIF_SUBCOMMAND_SHUTDOWN_GUEST;
	} else 	if (code == 21) {
		packet.header.command = CONTAINER_EXTIF_COMMAND_LIFECYCLE_GUEST_ROLE;
		packet.subcommand = CONTAINER_EXTIF_SUBCOMMAND_SHUTDOWN_GUEST;
	} else 	if (code == 22) {
		packet.header.command = CONTAINER_EXTIF_COMMAND_LIFECYCLE_GUEST_NAME;
		packet.subcommand = CONTAINER_EXTIF_SUBCOMMAND_REBOOT_GUEST;
	} else 	if (code == 23) {
		packet.header.command = CONTAINER_EXTIF_COMMAND_LIFECYCLE_GUEST_ROLE;
		packet.subcommand = CONTAINER_EXTIF_SUBCOMMAND_REBOOT_GUEST;
	} else 	if (code == 24) {
		packet.header.command = CONTAINER_EXTIF_COMMAND_LIFECYCLE_GUEST_NAME;
		packet.subcommand = CONTAINER_EXTIF_SUBCOMMAND_FORCEREBOOT_GUEST;
	} else 	if (code == 25) {
		packet.header.command = CONTAINER_EXTIF_COMMAND_LIFECYCLE_GUEST_ROLE;
		packet.subcommand = CONTAINER_EXTIF_SUBCOMMAND_FORCEREBOOT_GUEST;
	} else {
		goto error_return;
	}

	message_index = code - 20;

	(void) strncpy(packet.guest_name, name, (sizeof(packet.guest_name) - 1u));

	sret = write(fd, &packet, sizeof(packet));
	if (sret < (ssize_t)sizeof(packet)) {
		(void) fprintf(stderr,"Container manager is confuse.\n");
		goto error_return;
	}

	ret = cm_socket_wait_response(fd, 1000);
	if (ret < 0) {
		(void) fprintf(stderr,"Container manager communication is un available.\n");
		goto error_return;
	}

	sret = read(fd, &response, sizeof(response));
	if (sret < (ssize_t)sizeof(response)) {
		(void) fprintf(stderr,"Container manager is confuse. sret = %ld errno = %d\n", sret, errno);
		goto error_return;
	}

	if (response.header.command == CONTAINER_EXTIF_COMMAND_RESPONSE_LIFECYCLE) {

		if (response.response == CONTAINER_EXTIF_LIFECYCLE_RESPONSE_ACCEPT) {
			(void) fprintf(stdout, cm_control_lifecycle_messages[message_index], name);
		} else if (response.response == CONTAINER_EXTIF_LIFECYCLE_RESPONSE_NONAME) {
			(void) fprintf(stdout, "Unknown guest name: %s.\n", name);
		} else if (response.response == CONTAINER_EXTIF_LIFECYCLE_RESPONSE_NOROLE) {
			(void) fprintf(stdout, "Unknown guest role: %s.\n", name);
		} else if (response.response == CONTAINER_EXTIF_LIFECYCLE_RESPONSE_ERROR) {
			(void) fprintf(stdout, "Error response.\n");
		} else {
			(void) fprintf(stdout, "Unknown error.\n");
		}
	} else {
		(void) fprintf(stdout, "No response from container-manager\n");
	}

error_return:
	if (fd != -1) {
		(void) close(fd);
	}

	return;
}

void cm_get_guest_change(int code, char *name)
{
	int fd = -1;
	int ret = -1;
	ssize_t sret = -1;
	container_extif_command_change_t packet;
	container_extif_command_change_response_t response;

	(void) memset(&packet, 0, sizeof(packet));
	(void) memset(&response, 0, sizeof(response));

	// Create client socket
	fd = cm_socket_setup();
	if (fd < 0) {
		(void) fprintf(stderr,"Container manager is busy.\n");
		goto error_return;
	}

	if (code == 30) {
		packet.header.command = CONTAINER_EXTIF_COMMAND_CHANGE_ACTIVE_GUEST_NAME;
	} else {
		goto error_return;
	}

	(void) strncpy(packet.guest_name, name, sizeof(packet.guest_name)-1u);

	sret = write(fd, &packet, sizeof(packet));
	if (sret < (ssize_t)sizeof(packet)) {
		(void) fprintf(stderr,"Container manager is confuse.\n");
		goto error_return;
	}

	ret = cm_socket_wait_response(fd, 1000);
	if (ret < 0) {
		(void) fprintf(stderr,"Container manager communication is un available.\n");
		goto error_return;
	}

	sret = read(fd, &response, sizeof(response));
	if (sret < (ssize_t)sizeof(response)) {
		(void) fprintf(stderr,"Container manager is confuse. sret = %ld errno = %d\n", sret, errno);
		goto error_return;
	}

	if (response.header.command == CONTAINER_EXTIF_COMMAND_RESPONSE_CHANGE) {

		if (response.response == CONTAINER_EXTIF_CHANGE_RESPONSE_ACCEPT) {
			(void) fprintf(stdout, "Success to exchange active guest to %s.\n", name);
		} else if (response.response == CONTAINER_EXTIF_CHANGE_RESPONSE_NONAME) {
			(void) fprintf(stdout, "Guest name %s does not find.\n", name);
		} else if (response.response == CONTAINER_EXTIF_CHANGE_RESPONSE_ERROR) {
			(void) fprintf(stdout, "Error response.\n");
		} else {
			(void) fprintf(stdout, "Unknown error.\n");
		}
	}

error_return:
	if (fd != -1) {
		(void) close(fd);
	}

	return;
}

void cm_test_trigger(char *arg)
{
	int fd = -1;
	int ret = -1;
	ssize_t sret = -1;
	int value = 0;
	char *endptr = NULL;
	container_extif_command_test_trigger_t packet;
	container_extif_command_test_trigger_response_t response;

	(void) memset(&packet, 0, sizeof(packet));
	(void) memset(&response, 0, sizeof(response));

	value = (int)strtol(arg, &endptr, 10);
	if (endptr == arg) {
		// Convert fail.
		goto error_return;
	}

	// Create client socket
	fd = cm_socket_setup();
	if (fd < 0) {
		(void) fprintf(stderr,"Container manager is busy.\n");
		goto error_return;
	}

	packet.header.command = CONTAINER_EXTIF_COMMAND_TEST_TRIGGER;
	packet.code = (int32_t)value;
	sret = write(fd, &packet, sizeof(packet));
	if (sret < (ssize_t)sizeof(packet)) {
		(void) fprintf(stderr,"Container manager is confuse.\n");
		goto error_return;
	}

	ret = cm_socket_wait_response(fd, 1000);
	if (ret < 0) {
		(void) fprintf(stderr,"Container manager communication is un available.\n");
		goto error_return;
	}

	sret = read(fd, &response, sizeof(response));
	if (sret < (ssize_t)sizeof(response)) {
		(void) fprintf(stderr,"Container manager is confuse. sret = %ld errno = %d\n", sret, errno);
		goto error_return;
	}

	if (response.header.command == CONTAINER_EXTIF_COMMAND_RESPONSE_TEST_TRIGGER) {
		(void) fprintf(stderr,"Container manager return test trigger = %d\n", response.response);
	}

error_return:
	if (fd != -1) {
		(void) close(fd);
	}

	return;
}

int main(int argc, char *argv[])
{
	int ret = -1;

	do {
		ret = getopt_long(argc, argv, "", long_options, NULL);
		if (ret <= 1) {
			usage();
			break;
		} else if (ret == 10 || ret == 11) {
			if (ret == 11) {
				cm_get_guest_list(1);
			}
			else {
				cm_get_guest_list(0);
			}
			break;
		} else if (ret >= 20 && ret <= 25) {
			cm_get_guest_lifecycle(ret, optarg);
			break;
		} else if (ret == 30) {
			cm_get_guest_change(ret, optarg);
			break;
		} else if (ret == 90) {
			cm_test_trigger(optarg);
			break;
		} else {
			//TODO
			(void) fprintf(stderr, "This option is not support.\n");
			break;
		}
	} while(1);

	return 0;
}
