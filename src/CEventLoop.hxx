#ifndef	CEVENT_LOOP_H
#define	CEVENT_LOOP_H
//-----------------------------------------------------------------------------
#include "CBase.hxx"

#include <systemd/sd-event.h>


#include <libmnl/libmnl.h>

//-----------------------------------------------------------------------------
class CEventLoop: public CBase
{
private :
protected :
	//sd event
	sd_event *m_event;
	
	//netlink
	struct mnl_socket *m_nl;
	uint32_t m_seqnum;
	unsigned int m_nlportid;
	
	
	
	// methods
	virtual int SetupSignalHandling();
	virtual int SetupWatchdog();
	virtual int SetupNetdevScan();
	virtual int CleanupNetdevScan();

	virtual int LibmnlDataCallbackHandler(const struct nlmsghdr *nlh);
	virtual int SdEventIOHandlerNL(sd_event_source *s, int fd, uint32_t revents);
	
	
	//static method
	static int StaticLibmnlDataCallbackHandler(const struct nlmsghdr *nlh, void *data);
	static int StaticSdEventIOHandlerNL(sd_event_source *s, int fd, uint32_t revents, void *userdata);
	
public:
	// methods
	virtual int Create();
	virtual int Destroy();
	virtual int Run();

	
	CEventLoop();
	CEventLoop(const CEventLoop&) = delete;
	CEventLoop &operator = (const CEventLoop&) = delete;
	virtual ~CEventLoop();
};
//-----------------------------------------------------------------------------
#endif	//#ifndef	CEVENT_LOOP_H

