/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	ns-util.c
 * @brief	name space control utility functions
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
#define UEVENT_SEND 16
#endif

/**
 * Sub function for uevent monitor.
 * Get point to /dev/ trimmed devname.
 *
 * @param [in]	handle	Handle created by udevmonitor_setup;
 * @return int	 != NULL pointer to devname
 * 				 1 on the blacklist
 * 				-3 argument error
 *				-2 internal error
 *				-1 Mandatory data is nothing
 */
static char *trimmed_devname(char* devnode)
{
	char *cmpstr = "/dev/";
	char *pstr = NULL;
	int cmplen = 0;

	cmplen = strlen(cmpstr);

	if (strncmp(devnode, cmpstr, cmplen) == 0) {
		pstr = devnode;
		pstr += cmplen;
	}

	return pstr;
}
/**
 * Sub function for open name space.
 *
 * @param [in]	pid	target process pid
 * @param [in]	ns_name	name of open name space
 * @return int	 >=0 fd of pid's name space
 * 				-1 argument error
 *				-2 no name space or process
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
 * @param [in]	target_pid	target process pid
 * @param [in]	uenevt	uevent string
 * @return int	 >=0 fd of pid's name space
 * 				-1 argument error
 *				-2 Fail to get namespace
 */
static int uevent_injection_child(int net_ns_fd, const char *message, int messagesize)
{
	int result = -1;
	int ret = -1;
	int length = 0;
	struct mnl_socket *nl;
	struct nlmsghdr *nlh;
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
 * Sub function for open name space.
 *
 * @param [in]	target_pid	target process pid
 * @param [in]	uenevt	uevent string
 * @return int	 >=0 buf usage
 * 				-1 argument error
 *				-2 Fail to create uevent
 *				-3 Fail to fork
 */
static int uevent_injection_create_ueventdata(char *buf, int bufsize, dynamic_device_elem_data_t *dded, char *action)
{
	int ret = -1;
	int usage = 0, remain = 0;

	if (bufsize < 1)
		return -1;

	memset(buf, 0, bufsize);
	remain = bufsize;

	// add@/devices/pci0000:00/0000:00:08.1/0000:05:00.3/usb4/4-2/4-2:1.0/host3/target3:0:0/3:0:0:0/block/sdb/sdb1
	ret = snprintf(&buf[usage], remain, "%s@%s", action, dded->devpath);
	if ((!(ret < remain)) || ret < 0)
		return -2;

	#ifdef _PRINTF_DEBUG_
	fprintf(stderr, "add injection message : %s\n", &buf[usage]);
	#endif

	usage = usage + ret + 1 /*NULL term*/;
	remain = bufsize - usage;
	if (remain < 0)
		return -2;

	// ACTION=add
	ret = snprintf(&buf[usage], remain, "ACTION=%s", action);
	if ((!(ret < remain)) || ret < 0)
		return -2;

	#ifdef _PRINTF_DEBUG_
	fprintf(stderr, "add injection message : %s\n", &buf[usage]);
	#endif

	usage = usage + ret + 1 /*NULL term*/;
	remain = bufsize - usage;
	if (remain < 0)
		return -2;

	// DEVPATH=/devices/pci0000:00/0000:00:08.1/0000:05:00.3/usb4/4-2/4-2:1.0/host3/target3:0:0/3:0:0:0/block/sdb/sdb1
	ret = snprintf(&buf[usage], remain, "DEVPATH=%s", dded->devpath);
	if ((!(ret < remain)) || ret < 0)
		return -2;

	#ifdef _PRINTF_DEBUG_
	fprintf(stderr, "add injection message : %s\n", &buf[usage]);
	#endif

	usage = usage + ret + 1 /*NULL term*/;
	remain = bufsize - usage;
	if (remain < 0)
		return -2;

	// SUBSYSTEM=block
	ret = snprintf(&buf[usage], remain, "SUBSYSTEM=%s", dded->subsystem);
	if ((!(ret < remain)) || ret < 0)
		return -2;

	#ifdef _PRINTF_DEBUG_
	fprintf(stderr, "add injection message : %s\n", &buf[usage]);
	#endif

	usage = usage + ret + 1 /*NULL term*/;
	remain = bufsize - usage;
	if (remain < 0)
		return -2;

	// MAJOR=8 and MINOR=17
	if (dded->devnum != 0) {
		// MAJOR=8
		ret = snprintf(&buf[usage], remain, "MAJOR=%d", major(dded->devnum));
		if ((!(ret < remain)) || ret < 0)
			return -2;

		#ifdef _PRINTF_DEBUG_
		fprintf(stderr, "add injection message : %s\n", &buf[usage]);
		#endif

		usage = usage + ret + 1 /*NULL term*/;
		remain = bufsize - usage;
		if (remain < 0)
			return -2;

		// MINOR=17
		ret = snprintf(&buf[usage], remain, "MINOR=%d", minor(dded->devnum));
		if ((!(ret < remain)) || ret < 0)
			return -2;

		#ifdef _PRINTF_DEBUG_
		fprintf(stderr, "add injection message : %s\n", &buf[usage]);
		#endif

		usage = usage + ret + 1 /*NULL term*/;
		remain = bufsize - usage;
		if (remain < 0)
			return -2;
	}

	// DEVNAME=sdb1
	if (dded->devnode != NULL) {
		char *devname = NULL;
		devname = trimmed_devname(dded->devnode);
		if (devname != NULL) {
			ret = snprintf(&buf[usage], remain, "DEVNAME=%s", devname);
			if ((!(ret < remain)) || ret < 0)
				return -2;

			#ifdef _PRINTF_DEBUG_
			fprintf(stderr, "add injection message : %s\n", &buf[usage]);
			#endif

			usage = usage + ret + 1 /*NULL term*/;
			remain = bufsize - usage;
			if (remain < 0)
				return -2;
		}
	}

	// DEVTYPE=partition
	if (dded->devtype != NULL) {
		ret = snprintf(&buf[usage], remain, "DEVTYPE=%s", dded->devtype);
		if ((!(ret < remain)) || ret < 0)
			return -2;

		#ifdef _PRINTF_DEBUG_
		fprintf(stderr, "add injection message : %s\n", &buf[usage]);
		#endif

		usage = usage + ret + 1 /*NULL term*/;
		remain = bufsize - usage;
		if (remain < 0)
			return -2;
	}

	// DISKSEQ=23
	if (dded->diskseq != NULL) {
		ret = snprintf(&buf[usage], remain, "DISKSEQ=%s", dded->diskseq);
		if ((!(ret < remain)) || ret < 0)
			return -2;

		#ifdef _PRINTF_DEBUG_
		fprintf(stderr, "add injection message : %s\n", &buf[usage]);
		#endif

		usage = usage + ret + 1 /*NULL term*/;
		remain = bufsize - usage;
		if (remain < 0)
			return -2;
	}

	// PARTN=1
	if (dded->partn != NULL) {
		ret = snprintf(&buf[usage], remain, "PARTN=%s", dded->partn);
		if ((!(ret < remain)) || ret < 0)
			return -2;

		#ifdef _PRINTF_DEBUG_
		fprintf(stderr, "add injection message : %s\n", &buf[usage]);
		#endif

		usage = usage + ret + 1 /*NULL term*/;
		remain = bufsize - usage;
		if (remain < 0)
			return -2;
	}

	// SEQNUM=4341
	// not need it

	return usage;
}
/*
	message example

	add@/devices/pci0000:00/0000:00:08.1/0000:05:00.3/usb4/4-2/4-2:1.0/host3/target3:0:0/3:0:0:0/block/sdb
		ACTION=add
		DEVPATH=/devices/pci0000:00/0000:00:08.1/0000:05:00.3/usb4/4-2/4-2:1.0/host3/target3:0:0/3:0:0:0/block/sdb
		SUBSYSTEM=block
		MAJOR=8
		MINOR=16
		DEVNAME=sdb
		DEVTYPE=disk
		DISKSEQ=23
		SEQNUM=4340

	add@/devices/pci0000:00/0000:00:08.1/0000:05:00.3/usb4/4-2/4-2:1.0/host3/target3:0:0/3:0:0:0/block/sdb/sdb1
		ACTION=add
		DEVPATH=/devices/pci0000:00/0000:00:08.1/0000:05:00.3/usb4/4-2/4-2:1.0/host3/target3:0:0/3:0:0:0/block/sdb/sdb1
		SUBSYSTEM=block
		MAJOR=8
		MINOR=17
		DEVNAME=sdb1
		DEVTYPE=partition
		DISKSEQ=23
		PARTN=1
		SEQNUM=4341

	remove@/devices/pci0000:00/0000:00:08.1/0000:05:00.3/usb4/4-2/4-2:1.0/host3/target3:0:0/3:0:0:0/block/sdb/sdb1
		ACTION=remove
		DEVPATH=/devices/pci0000:00/0000:00:08.1/0000:05:00.3/usb4/4-2/4-2:1.0/host3/target3:0:0/3:0:0:0/block/sdb/sdb1
		SUBSYSTEM=block
		MAJOR=8
		MINOR=17
		DEVNAME=sdb1
		DEVTYPE=partition
		DISKSEQ=23
		PARTN=1
		SEQNUM=4347

	remove@/devices/pci0000:00/0000:00:08.1/0000:05:00.3/usb4/4-2/4-2:1.0/host3/target3:0:0/3:0:0:0/block/sdb
		ACTION=remove
		DEVPATH=/devices/pci0000:00/0000:00:08.1/0000:05:00.3/usb4/4-2/4-2:1.0/host3/target3:0:0/3:0:0:0/block/sdb
		SUBSYSTEM=block
		MAJOR=8
		MINOR=16
		DEVNAME=sdb
		DEVTYPE=disk
		DISKSEQ=23
		SEQNUM=4349
*/

/**
 * Sub function for open name space.
 *
 * @param [in]	target_pid	target process pid
 * @param [in]	uenevt	uevent string
 * @return int	 >=0 fd of pid's name space
 * 				-1 argument error
 *				-2 Fail to get namespace
 *				-3 Fail to fork
 */
int uevent_injection_to_pid(pid_t target_pid, dynamic_device_elem_data_t *dded, char *action)
{
	int result = -1;
	int ret = -1;
	int net_ns_fd = -1;
	pid_t child_pid = -1;
	int messagesize = 0;
	char buf[MNL_SOCKET_BUFFER_SIZE];

	if (target_pid < 1 || dded == NULL || action == NULL)
		return -1;

	net_ns_fd = open_namespace_fd(target_pid, "net");
	if (net_ns_fd < 0) {
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr, "open_namespace_fd fail pid = %d\n", target_pid);
		#endif
		result = -2;
		goto err_return;
	}

	messagesize = uevent_injection_create_ueventdata(buf, MNL_SOCKET_BUFFER_SIZE, dded, action);
	if (messagesize < 0) {
		result = -1;
		goto err_return;
	}

	child_pid = fork();
	if (child_pid < 0) {
		result = -3;
		goto err_return;
	}

	if (child_pid == 0) {
		// run on child process, must be exit.
		ret = uevent_injection_child(net_ns_fd, buf, messagesize);
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
