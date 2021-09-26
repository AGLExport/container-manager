#include "CEventLoop.hxx"

#include <signal.h>
#include <pthread.h>

#include <linux/if.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>


#include <stdio.h>
#include <string.h>

/*
#include <string>
#include <iostream>
#include <fstream>

#include <stdlib.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
*/

//-----------------------------------------------------------------------------
CEventLoop::CEventLoop() : m_event(nullptr),m_nl(nullptr),m_seqnum(0),m_nlportid(0)
{
}
//-----------------------------------------------------------------------------
CEventLoop::~CEventLoop()
{
}
//-----------------------------------------------------------------------------
int CEventLoop::Create()
{
	int ret = 0;
	
	if (this->m_event != nullptr)
		return -1;
	
	ret = sd_event_default(&this->m_event);
	if (ret < 0)
		return -1;
	
	ret = SetupSignalHandling();
	if (ret < 0) {
		this->m_event = sd_event_unref(this->m_event);
		return -1;
	}
	
	ret = SetupWatchdog();
	if (ret < 0) {
		this->m_event = sd_event_unref(this->m_event);
		return -1;
	}
	
	ret = SetupNetdevScan();
	if (ret < 0) {
		this->m_event = sd_event_unref(this->m_event);
		return -1;
	}
	
	return 0;
}
//-----------------------------------------------------------------------------
int CEventLoop::Destroy()
{
	int ret;
	
	if (this->m_event == nullptr)
		return -1;
	
	this->m_event = sd_event_unref(this->m_event);
	
	

	return 0;
}
//-----------------------------------------------------------------------------
int CEventLoop::SetupSignalHandling()
{
	int ret;
	sigset_t mask;
	
	// signal mask
	::sigemptyset(&mask);
	::sigaddset(&mask, SIGPIPE);
	::sigaddset(&mask, SIGTERM);
	
	::pthread_sigmask(SIG_BLOCK, &mask, nullptr);

	ret = sd_event_add_signal(this->m_event, nullptr, SIGTERM, nullptr, nullptr);
	
	return ret;
}
//-----------------------------------------------------------------------------
int CEventLoop::SetupWatchdog()
{
	int ret;
	
	ret = sd_event_set_watchdog(this->m_event,true);
	fprintf(stdout,"sd_event_set_watchdog = %d\n",ret);
	
	return ret;
}
//-----------------------------------------------------------------------------
int CEventLoop::SetupNetdevScan()
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct rtgenmsg *rt;
	int ret;
	unsigned int seq, portid;

	this->m_seqnum = uint32_t(getpid());
	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_GETLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nlh->nlmsg_seq = this->m_seqnum;
	rt = (struct rtgenmsg *)mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtgenmsg));
	rt->rtgen_family = AF_PACKET;

	this->m_nl = mnl_socket_open2(NETLINK_ROUTE,SOCK_CLOEXEC);
	if (this->m_nl == NULL)
		return -1;
	
	ret = mnl_socket_bind(this->m_nl,RTMGRP_LINK,MNL_SOCKET_AUTOPID);
	if (ret < 0) {
		mnl_socket_close(this->m_nl);
		this->m_nl = nullptr;
		return -1;
	}
	this->m_nlportid = mnl_socket_get_portid(this->m_nl);

	ret = mnl_socket_sendto(this->m_nl, nlh, nlh->nlmsg_len);
	if (ret < 0) {
		mnl_socket_close(this->m_nl);
		this->m_nl = nullptr;
		return -1;
	}
	
	int fd = mnl_socket_get_fd(this->m_nl);
	
	ret = sd_event_add_io(this->m_event, nullptr, fd, EPOLLIN, CEventLoop::StaticSdEventIOHandlerNL, (void*)(this));
	
	return 0;
}
//-----------------------------------------------------------------------------
int CEventLoop::CleanupNetdevScan()
{
	mnl_socket_close(this->m_nl);
	this->m_nl = nullptr;
	
	return 0;
}
//-----------------------------------------------------------------------------
static int data_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = (const struct nlattr **)data;
	int type = mnl_attr_get_type(attr);

	// skip unsupported attribute in user-space
	if (mnl_attr_type_valid(attr, IFLA_MAX) < 0)
		return MNL_CB_OK;

	switch(type) {
	case IFLA_ADDRESS:
		if (mnl_attr_validate(attr, MNL_TYPE_BINARY) < 0) {
			//perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	case IFLA_MTU:
		if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0) {
			//perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	case IFLA_IFNAME:
		if (mnl_attr_validate(attr, MNL_TYPE_STRING) < 0) {
			//perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	/*case IFLA_LINK:
		if (mnl_attr_validate(attr, MNL_TYPE_STRING) < 0) {
			//perror("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break*/
	}
	tb[type] = attr;
	return MNL_CB_OK;
}
//-----------------------------------------------------------------------------
int CEventLoop::LibmnlDataCallbackHandler(const struct nlmsghdr *nlh)
{
	struct nlattr *tb[IFLA_MAX+1];
	struct ifinfomsg *ifm = (struct ifinfomsg *)mnl_nlmsg_get_payload(nlh);

	::memset(tb,0,sizeof(tb));
	
	printf("index=%d type=%d flags=%d family=%d ", 
		ifm->ifi_index, ifm->ifi_type,
		ifm->ifi_flags, ifm->ifi_family);

	if (ifm->ifi_flags & IFF_RUNNING)
		printf("[RUNNING] ");
	else
		printf("[NOT RUNNING] ");

	mnl_attr_parse(nlh, sizeof(*ifm), data_attr_cb, tb);
	if (tb[IFLA_MTU]) {
		printf("mtu=%d ", mnl_attr_get_u32(tb[IFLA_MTU]));
	}
	if (tb[IFLA_IFNAME]) {
		printf("name=%s ", mnl_attr_get_str(tb[IFLA_IFNAME]));
	}
	if (tb[IFLA_ADDRESS]) {
		uint8_t *hwaddr = (uint8_t *)mnl_attr_get_payload(tb[IFLA_ADDRESS]);
		int i;

		printf("hwaddr=");
		for (i=0; i<mnl_attr_get_payload_len(tb[IFLA_ADDRESS]); i++) {
			printf("%.2x", hwaddr[i] & 0xff);
			if (i+1 != mnl_attr_get_payload_len(tb[IFLA_ADDRESS]))
				printf(":");
		}
		printf(" ");
	}
	if (tb[IFLA_EVENT]) {
		printf("link=%d ", mnl_attr_get_u32(tb[IFLA_LINK]));
	}
	if (tb[IFLA_BROADCAST]) {
		uint8_t *hwaddr = (uint8_t *)mnl_attr_get_payload(tb[IFLA_BROADCAST]);
		int i;

		printf("hwaddr=");
		for (i=0; i<mnl_attr_get_payload_len(tb[IFLA_BROADCAST]); i++) {
			printf("%.2x", hwaddr[i] & 0xff);
			if (i+1 != mnl_attr_get_payload_len(tb[IFLA_BROADCAST]))
				printf(":");
		}
		printf(" ");
	}
	
	printf("\n");
	return MNL_CB_OK;
}
//-----------------------------------------------------------------------------
int CEventLoop::StaticLibmnlDataCallbackHandler(const struct nlmsghdr *nlh, void *data)
{
	CEventLoop *pev = NULL;
	
	if (data == NULL)
		return 0;
	
	pev = (CEventLoop*)(data);
	
	return pev->LibmnlDataCallbackHandler(nlh);
}
//-----------------------------------------------------------------------------
int CEventLoop::SdEventIOHandlerNL(sd_event_source *s, int fd, uint32_t revents)
{
	int ret;
	char buf[MNL_SOCKET_BUFFER_SIZE];
	
	ret = mnl_socket_recvfrom(this->m_nl, buf, sizeof(buf));
	while (ret > 0){
		ret = mnl_cb_run(buf, ret, this->m_seqnum, this->m_nlportid, CEventLoop::StaticLibmnlDataCallbackHandler, (void*)(this));
		if (ret <= MNL_CB_STOP)
			break;
		ret = mnl_socket_recvfrom(this->m_nl, buf, sizeof(buf));
	}
	
	if (ret < 0)
		return 0;
	
	return 1;
}
//-----------------------------------------------------------------------------
int CEventLoop::StaticSdEventIOHandlerNL(sd_event_source *s, int fd, uint32_t revents, void *userdata)
{
	CEventLoop *pev = NULL;
	
	if (userdata == NULL)
		return 0;
	
	pev = (CEventLoop*)(userdata);
	
	return pev->SdEventIOHandlerNL(s,fd,revents);
}
//-----------------------------------------------------------------------------
int CEventLoop::Run()
{
	int ret;
	
	ret = sd_event_loop(this->m_event);
	
	fprintf(stdout,"sd_event_loop = %d\n",ret);
	
	return ret;
}
//-----------------------------------------------------------------------------


