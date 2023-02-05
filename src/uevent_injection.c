/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	uevent_injection.c
 * @brief	This file implement to utility functions for uevent injection.
 */

#undef _PRINTF_DEBUG_

#include "uevent_injection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>
#include <libmnl/libmnl.h>
#include <sys/sysmacros.h>

#include "cm-utils.h"

#ifndef UEVENT_SEND
/**
 * @def	UEVENT_SEND
 * @brief	A nl message type of uevent injection.
 */
#define UEVENT_SEND 16
#endif

/**
 * Sub function for open name space.
 *
 * @param [in]	pid	target process pid
 * @param [in]	ns_name	name of open name space
 * @return int
 * @retval	>=0	A fd of 'ns_mname' name space.
 * @retval	-1	Argument error.
 * @retval	-2	No name space or process.
 */
static int open_namespace_fd(pid_t pid, const char *ns_name)
{
	int ret = -1;
	char buf[1024];

	ret = snprintf(buf, sizeof(buf), "/proc/%d/ns/%s", pid, ns_name);
	if (!(ret < sizeof(buf)))
		return -1;

	ret = open(buf, (O_RDONLY|O_CLOEXEC));
	if (ret < 0)
		return -2;

	return ret;
}
/**
 * Sub function for open name space.
 *
 * @param [in]	net_ns_fd	A fd of network namespace for guest container.
 * @param [in]	message		Injecting message data.
 * @param [in]	messagesize	Injecting message data size.
 * @return int
 * @retval	0	Success to inject uevent message.
 * @retval	-1	Internal error.
 */
static int uevent_injection_child(int net_ns_fd, const char *message, int messagesize)
{
	int result = -1;
	int ret = -1;
	struct mnl_socket *nl = NULL;
	struct nlmsghdr *nlh = NULL;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	char *pevmessage = NULL;

	// create injection message
	memset(buf, 0 , sizeof(buf));

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= UEVENT_SEND;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_pid = 0;

	pevmessage = mnl_nlmsg_put_extra_header(nlh, messagesize);
	memcpy(pevmessage, message, messagesize);

	// event injection
	ret = setns(net_ns_fd, CLONE_NEWNET);
	close(net_ns_fd);
	if (ret < 0) {
		result = -1;
		goto err_return;
	}

	nl = mnl_socket_open2(NETLINK_KOBJECT_UEVENT, SOCK_CLOEXEC);
	if (nl == NULL) {
		result = -1;
		goto err_return;
	}

	/* There is one single group in kobject over netlink */
	if (mnl_socket_bind(nl, (1<<0), MNL_SOCKET_AUTOPID) < 0) {
		result = -1;
		goto err_return;
	}

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		result = -1;
		goto err_return;
	}

	mnl_socket_close(nl);

	return 0;

err_return:
	if (nl != NULL)
		mnl_socket_close(nl);

	if (net_ns_fd >= 0)
		close(net_ns_fd);

	return result;
}
/**
 * Inject uevent to guest container using pid.
 *
 * @param [in]	target_pid	Target process pid.
 * @param [in]	dded		Pointer to dynamic_device_elem_data_t that include injecting device information.
 * @param [in]	action		String for device action. (add/remove)
 * @return int
 * @retval	0	Success to inject uevent message.
 * @retval	-1	Argument error.
 * @retval	-2	Too large created uevent message.
 * @retval	-3	Fork error.
 * @retval	-4	Error from child process.
 */
int uevent_injection_to_pid(pid_t target_pid, uevent_injection_message_t *uim)
{
	int result = -1;
	int ret = -1;
	int net_ns_fd = -1;
	pid_t child_pid = -1;

	if (target_pid < 1 || uim == NULL)
		return -1;

	net_ns_fd = open_namespace_fd(target_pid, "net");
	if (net_ns_fd < 0) {
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr, "open_namespace_fd fail pid = %d\n", target_pid);
		#endif
		result = -2;
		goto err_return;
	}

	child_pid = fork();
	if (child_pid < 0) {
		result = -3;
		goto err_return;
	}

	if (child_pid == 0) {
		// run on child process, must be exit.
		ret = uevent_injection_child(net_ns_fd, uim->message, uim->used);
		if (ret < 0)
			_exit(EXIT_FAILURE);

		_exit(EXIT_SUCCESS);
	}

	ret = wait_child_pid(child_pid);
	if (ret < 0) {
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr, "uevent_injection_child was fail\n");
		#endif
		result = -4;
		goto err_return;
	}

	close(net_ns_fd);

	return 0;

err_return:

	if (net_ns_fd >= 0)
		close(net_ns_fd);

	return result;
}
