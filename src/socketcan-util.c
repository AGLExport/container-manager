/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	socketcan-util.c
 * @brief	This file include socketcan utility functions using libnml.
 */
#include "socketcan-util.h"

#include <stdlib.h>

#include <libmnl/libmnl.h>
#include <linux/if.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>
#include <linux/can/vxcan.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <stdio.h>
#include <string.h>

#undef _PRINTF_DEBUG_

/**
 * Sub function for getting network interface name.
 * This function call from data_cb.
 *
 * @param [in]	nlh		The nlmsghdr by libmnl.
 * @param [out]	ifname	Buffer for ifname.
 * @param [out]	size	Buffer size for ifname.
 * @return int
 * @retval	0	Success to get available data.
 * @retval	-1	No data.
 * @retval	-2	Argument error.
 */
int socketcanutil_create_vxcan_peer(const char *ifname, const char *peer_ifname)
{
	struct mnl_socket *nl = NULL;
	char buf[8192];
	struct nlmsghdr *nlh = NULL;
	struct ifinfomsg *ifm = NULL;
	struct nlattr *linkinfo = NULL;
	int ret = 0, result = 0;
	unsigned int portid = 0, vxcan_seq = 115200;

	if ((ifname == NULL) || (peer_ifname == NULL)) {
		return -2;
	}

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_NEWLINK;
	nlh->nlmsg_flags = (NLM_F_REQUEST | NLM_F_ACK | NLM_F_EXCL | NLM_F_CREATE);
	nlh->nlmsg_seq = vxcan_seq;
	ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_change = 0;
	ifm->ifi_flags = 0;

	mnl_attr_put_str(nlh, IFLA_IFNAME, ifname);
	linkinfo = mnl_attr_nest_start(nlh, IFLA_LINKINFO);
	{
		struct nlattr *peerinfo = NULL;
		mnl_attr_put_str(nlh, IFLA_INFO_KIND, "vxcan");
		peerinfo = mnl_attr_nest_start(nlh, IFLA_INFO_DATA);
		{
			struct nlattr *vxcaninfo = NULL;
			vxcaninfo = mnl_attr_nest_start(nlh, VXCAN_INFO_PEER);
			{
				struct ifinfomsg *peer_ifm = NULL;
				peer_ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*peer_ifm));
				peer_ifm->ifi_family = AF_UNSPEC;
				peer_ifm->ifi_change = 0;
				peer_ifm->ifi_flags = 0;
				mnl_attr_put_str(nlh, IFLA_IFNAME, peer_ifname);
			}
			mnl_attr_nest_end(nlh, vxcaninfo);
		}
		mnl_attr_nest_end(nlh, peerinfo);
	}
	mnl_attr_nest_end(nlh, linkinfo);

	nl = mnl_socket_open(NETLINK_ROUTE);
	if (nl == NULL) {
		result = -1;
		goto do_return;
	}

	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		result = -1;
		goto do_return;
	}
	portid = mnl_socket_get_portid(nl);

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		result = -1;
		goto do_return;
	}

	ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	if (ret == -1) {
		result = -1;
		goto do_return;
	}

	ret = mnl_cb_run(buf, ret, vxcan_seq, portid, NULL, NULL);
	if (ret == -1){
		result = -1;
		goto do_return;
	}

do_return:
	if (nl != NULL) {
		mnl_socket_close(nl);
	}

	return result;
}
