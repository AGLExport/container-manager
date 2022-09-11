/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	net-util.c
 * @brief	netlink utility functions
 */
#include "net-util.h"

#include <stdlib.h>
#include <systemd/sd-event.h>

#include <libmnl/libmnl.h>
#include <linux/if.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "list.h"

#include <stdio.h>
#include <string.h>

#undef _PRINTF_DEBUG_

struct s_netifmonitor {
	struct mnl_socket *nl;
	sd_event_source *ifmonitor_source;
	container_control_interface_t *cci;
};

#ifdef _PRINTF_DEBUG_
static void print_iflist(network_interface_manager_t *nfm)
{
	network_interface_info_t *ifinfo = NULL;

	dl_list_for_each(ifinfo, &nfm->nllist, network_interface_info_t, list) {
		fprintf(stderr, "index: %d  name: %s\n", ifinfo->ifindex, ifinfo->ifname);
	}
}
#endif //#ifdef _PRINTF_DEBUG_

static int network_interface_info_free(network_interface_info_t *nfi);

/**
 * Sub function for UNIX signal handling.
 * Block SIGTERM, when this process receive SIGTERM, event loop will exit.
 *
 * @param [in]	nlh	nlmsghdr
 * @param [out]	ifname	buffer for ifname
 * @return int	 0 available data
 * 				-2 argument error
 *				-1 no data
 */
static int sdutil_get_ifname(const struct nlmsghdr *nlh, char *ifname)
{
	struct ifinfomsg *ifm = mnl_nlmsg_get_payload(nlh);
	int type = 0, ret = 0, result = -1;
	struct nlattr *attr = NULL;
	const char *pstr = NULL;

	if (nlh == NULL || ifname == NULL)
		return -2;

	mnl_attr_for_each(attr, nlh, sizeof(*ifm)) {
		// skip unsupported attribute in user-space
		ret = mnl_attr_type_valid(attr, IFLA_MAX);
		if (ret < 0)
			continue;

		type = mnl_attr_get_type(attr);

		if (type == IFLA_IFNAME) {
			ret = mnl_attr_validate(attr, MNL_TYPE_STRING);
			if (ret == 0) {
				pstr = mnl_attr_get_str(attr);
				strncpy(ifname, pstr, sizeof(ifname));
				result = 0;
			}
			break;

		} else {
			//non operations
		}
	}

	return result;
}

/**
 * Sub function for UNIX signal handling.
 * Block SIGTERM, when this process receive SIGTERM, event loop will exit.
 *
 * @param [in]	event	sd_event
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
static int data_cb(const struct nlmsghdr *nlh, void *data)
{
	struct ifinfomsg *ifm = mnl_nlmsg_get_payload(nlh);
	network_interface_manager_t *netif = (network_interface_manager_t*)data;
	network_interface_info_t *nfi_new = NULL;
	int ret = 0;
	int ifindex = 0;
	char ifname[IFNAMSIZ+1];

	memset(ifname, 0, sizeof(ifname));

	ifindex = ifm->ifi_index; //Get if index
	ret = sdutil_get_ifname(nlh, ifname); //Get if name
	if (ret < 0)
		goto out; //no data

	nfi_new = (network_interface_info_t*)malloc(sizeof(network_interface_info_t));
	if (nfi_new == NULL)
		goto out;

	memset(nfi_new, 0, sizeof(network_interface_info_t));
	nfi_new->ifindex = ifindex;
	memcpy(nfi_new->ifname, ifname, sizeof(nfi_new->ifname));
	dl_list_init(&nfi_new->list);

	if (nlh->nlmsg_type == RTM_NEWLINK) {
		network_interface_info_t *nfi = NULL, *nfi_n = NULL;

		dl_list_for_each_safe(nfi, nfi_n, &netif->nllist, network_interface_info_t, list) {

			if (nfi->ifindex == nfi_new->ifindex) {
				// existing device is found -> remove
				#ifdef _PRINTF_DEBUG_
				fprintf(stderr,"netifmonitor-new: Found existing if \n exist = %s(%d)\n new = %s(%d)\n\n"
						, nfi->ifname, nfi->ifindex, nfi_new->ifname, nfi_new->ifindex);
				#endif
				dl_list_del(&nfi->list);
				network_interface_info_free(nfi);
			}
		}

		dl_list_add(&netif->nllist, &nfi_new->list);

	} else if (nlh->nlmsg_type == RTM_DELLINK) {
		network_interface_info_t *nfi = NULL, *nfi_n = NULL;

		dl_list_for_each_safe(nfi, nfi_n, &netif->nllist, network_interface_info_t, list) {

			if (nfi->ifindex == nfi_new->ifindex) {
				// existing device is found -> remove
				#ifdef _PRINTF_DEBUG_
				fprintf(stderr,"netifmonitor-del: Found existing if \n exist = %s(%d)\n new = %s(%d)\n\n"
						, nfi->ifname, nfi->ifindex, nfi_new->ifname, nfi_new->ifindex);
				#endif
				dl_list_del(&nfi->list);
				network_interface_info_free(nfi);
			}
		}

		network_interface_info_free(nfi_new);
	} else {
		; //no update
	}

out:
	return MNL_CB_OK;
}

static int nml_event_handler(sd_event_source *event, int fd, uint32_t revents, void *userdata)
{
	char buf[8192];
	int ret = 0;
	dynamic_device_manager_t *ddm = NULL;
	struct s_netifmonitor *nfm = NULL;
	container_control_interface_t *cci = NULL;
	struct mnl_socket *nl = NULL;

	if (userdata == NULL) {
		// Fail safe - disable udev event
		sd_event_source_disable_unref(event);
		return 0;
	}

	ddm = (dynamic_device_manager_t*)userdata;
	nfm = (struct s_netifmonitor*)ddm->netifmon;
	nl = nfm->nl;
	cci = nfm->cci;

	if ((revents & (EPOLLHUP | EPOLLERR)) != 0) {
		// Fail safe - disable udev event
		sd_event_source_disable_unref(event);
	} else if ((revents & EPOLLIN) != 0) {
		// Receive
		ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
		if (ret > 0) {
			(void)mnl_cb_run(buf, ret, 0, 0, data_cb, &ddm->netif);

			// Update notification
			(void)cci->netif_updated(cci);
		}
	}

	#ifdef _PRINTF_DEBUG_
	print_iflist(&ddm->netif);
	#endif	//#ifdef _PRINTF_DEBUG_

	return 0;
}
/**
 * Sub function for network if monitor.
 * List up for the existing network if.
 *
 * @param [in]	handle	Pointer to variable of dynamic_device_manager_t;
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
static int netifmonitor_listing_existif(dynamic_device_manager_t *ddm)
{
	char buf[8192];
	struct mnl_socket *nl = NULL;
	struct nlmsghdr *nlh = NULL;
	struct rtgenmsg *rt = NULL;
	unsigned int seq = 8192;
	unsigned int portid = 0;
	int ret = -1;

	nl = mnl_socket_open2(NETLINK_ROUTE, SOCK_CLOEXEC);
	if (nl == NULL)
		goto errorret;

	ret = mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID);
	if (ret < -1)
		goto errorret;

	// listing request
	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_GETLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nlh->nlmsg_seq = seq;
	rt = mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtgenmsg));
	rt->rtgen_family = AF_PACKET;

	portid = mnl_socket_get_portid(nl);

	ret = mnl_socket_sendto(nl, nlh, nlh->nlmsg_len);
	if (ret < -1)
		goto errorret;

	ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, seq, portid, data_cb, &ddm->netif);
		if (ret <= MNL_CB_STOP)
			break;
		ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	}

	if (ret == -1)
		goto errorret;

	mnl_socket_close(nl);

	#ifdef _PRINTF_DEBUG_
	print_iflist(&ddm->netif);
	#endif	//#ifdef _PRINTF_DEBUG_

	return 0;

errorret:
	if (nl !=NULL)
		mnl_socket_close(nl);

	return -1;
}

/**
 * Sub function for network if monitor.
 * Setup for the network if monitor event loop.
 *
 * @param [in]	handle	Pointer to variable of sdutil_netifmonitor_t;
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
int netifmonitor_setup(dynamic_device_manager_t *ddm, container_control_interface_t *cci, sd_event *event)
{
	struct s_netifmonitor *netifmon = NULL;
	struct mnl_socket *nl = NULL;
	sd_event_source *ifmonitor_source = NULL;
	int ret = -1;
	int fd = -1;

	if (ddm == NULL || cci == NULL || event == NULL)
		return -2;
	
	netifmon = malloc(sizeof(struct s_netifmonitor));
	if (netifmon == NULL)
		goto err_return;

	memset(netifmon,0,sizeof(struct s_netifmonitor));

	nl = mnl_socket_open2(NETLINK_ROUTE, SOCK_CLOEXEC | SOCK_NONBLOCK);
	if (nl == NULL)
		goto err_return;

	ret = mnl_socket_bind(nl, RTMGRP_LINK, MNL_SOCKET_AUTOPID);
	if (ret < -1)
		goto err_return;

	fd = mnl_socket_get_fd(nl);

	ret = sd_event_add_io(event, &ifmonitor_source, fd, EPOLLIN, nml_event_handler, ddm);
	if (ret < 0)
		goto err_return;

	netifmon->nl = nl;
	netifmon->ifmonitor_source = ifmonitor_source;
	netifmon->cci = cci;
	dl_list_init(&ddm->netif.nllist);

	ddm->netifmon = (netifmonitor_t*)netifmon;

	ret = netifmonitor_listing_existif(ddm);
	if (ret < 0)
		goto err_return;

	return 0;

err_return:
	if (nl != NULL)
		mnl_socket_close(nl);

	if (netifmon != NULL)
		free(netifmon);

	return -1;
}
/**
 * Sub function for network if monitor.
 * Cleanup for the network if monitor event loop.
 * Shall be call after sd_event_loop exit.
 *
 * @param [in]	ddm	dynamic_device_manager_t created by netifmonitor_setup;
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
int netifmonitor_cleanup(dynamic_device_manager_t *ddm)
{
	struct s_netifmonitor *netifmon = NULL;

	if (ddm == NULL)
		return -2;

	{
		network_interface_info_t *nfi = NULL, *nfi_n = NULL;

		dl_list_for_each_safe(nfi, nfi_n, &ddm->netif.nllist, network_interface_info_t, list) {
			dl_list_del(&nfi->list);
			network_interface_info_free(nfi);
		}
	}

	netifmon = (struct s_netifmonitor*)ddm->netifmon;

	if (netifmon->ifmonitor_source != NULL)
		(void)sd_event_source_disable_unref(netifmon->ifmonitor_source);

	if (netifmon->nl != NULL)
		mnl_socket_close(netifmon->nl);

	free(netifmon);

	return 0;
}
/**
 * Sub function for network if monitor.
 * Cleanup for the network_interface_info_t.
 *
 * @param [in]	nfi	pointer to network_interface_info_t.
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
static int network_interface_info_free(network_interface_info_t *nfi)
{
	if (nfi == NULL)
		return -2;

	free(nfi);

	return 0;
}
/**
 * Sub function for uevent monitor.
 * Cleanup for the dynamic_device_info_t data.
 *
 * @param [in]	handle	Handle created by udevmonitor_setup;
 * @return int	 0 success
 * 				-1 argument error
 */
int network_interface_info_get(network_interface_manager_t **netif, dynamic_device_manager_t *ddm)
{
	if (netif == NULL || ddm == NULL)
		return -1;

	(*netif) = &ddm->netif;

	return 0;
}
