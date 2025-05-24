/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	socketcan-util.c
 * @brief	This file include socketcan utility functions using libnml.
 */
#include "socketcan-util.h"

#include <stdlib.h>
#include <net/if.h>
#include <libmnl/libmnl.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>
#include <linux/can/vxcan.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <stdio.h>
#include <string.h>

#include <libsocketcangw.h>

#undef _PRINTF_DEBUG_

/**
 * Function for create VXCAN interface pair.
 *
 * @param [in]	ifname	Pointer to ifname.
 * @param [in]	peer_ifname	Pointer to peer ifname.
 * @return int
 * @retval	0	Success to create VXCAN interface pair.
 * @retval	-1	Fail to create VXCAN interface pair.
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
		result = -2;
		goto do_return;
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
/**
 * Function for remove VXCAN interface pair.
 *
 * @param [in]	ifname	Pointer to ifname.
 * @return int
 * @retval	0	Success to remove vxcan interface.
 * @retval	-1	Fail to remove VXCAN interface pair.
 * @retval	-2	Argument error.
 * @retval	-3	No interface.
 */
int socketcanutil_up_can_if(const char *ifname)
{
	struct mnl_socket *nl = NULL;
	char buf[8192];
	struct nlmsghdr *nlh = NULL;
	struct ifinfomsg *ifm = NULL;
	int ret = 0, result = 0;
	unsigned int portid = 0, vxcan_seq = 115202, vxcan_if_index = 0;

	if (ifname == NULL) {
		result = -2;
		goto do_return;
	}

	vxcan_if_index = if_nametoindex(ifname);
	if (vxcan_if_index == 0) {
		// No interface
		result = -3;
		goto do_return;
	}

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_NEWLINK;
	nlh->nlmsg_flags = (NLM_F_REQUEST | NLM_F_ACK);
	nlh->nlmsg_seq = vxcan_seq;
	ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_index = vxcan_if_index;
	ifm->ifi_change = 0;
	ifm->ifi_flags = IFF_UP;

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
/**
 * Function for remove VXCAN interface pair.
 *
 * @param [in]	ifname	Pointer to ifname.
 * @return int
 * @retval	0	Success to remove vxcan interface.
 * @retval	-1	Fail to remove VXCAN interface pair.
 * @retval	-2	Argument error.
 * @retval	-3	No interface.
 */
int socketcanutil_remove_vxcan_peer(const char *ifname)
{
	struct mnl_socket *nl = NULL;
	char buf[8192];
	struct nlmsghdr *nlh = NULL;
	struct ifinfomsg *ifm = NULL;
	int ret = 0, result = 0;
	unsigned int portid = 0, vxcan_seq = 115201, vxcan_if_index = 0;

	if (ifname == NULL) {
		result = -2;
		goto do_return;
	}

	vxcan_if_index = if_nametoindex(ifname);
	if (vxcan_if_index == 0) {
		// No interface
		result = -3;
		goto do_return;
	}

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_DELLINK;
	nlh->nlmsg_flags = (NLM_F_REQUEST | NLM_F_ACK);
	nlh->nlmsg_seq = vxcan_seq;
	ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_index = vxcan_if_index;
	ifm->ifi_change = 0;
	ifm->ifi_flags = 0;

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

int socketcanutil_configure_gateway(const char *src_ifname, const char *dest_ifname)
{
	int ret = -1;
	socketcan_gw_rule_t gw_rule;

	if ((src_ifname == NULL) || (dest_ifname == NULL)) {
		return -1;
	}

	memset(&gw_rule, 0, sizeof(gw_rule));

	gw_rule.src_ifindex = if_nametoindex(src_ifname);
	gw_rule.dst_ifindex = if_nametoindex(dest_ifname);

	if ((gw_rule.src_ifindex == 0) || (gw_rule.dst_ifindex == 0)) {
		return -2;
	}

	gw_rule.options |= SOCKETCAN_GW_RULE_FILTER;
	gw_rule.filter.can_id = 0x000;
	gw_rule.filter.can_mask = 0x000;

	ret = cangw_add_rule(&gw_rule);
	if (ret < 0) {
		; //nop
	}

	return 0;
}