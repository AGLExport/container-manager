/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-manager.c
 * @brief	main source file for container-manager
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "signal-util.h"
#include "udev-util.h"
#include "net-util.h"
#include "lxc-util.h"
#include "block-util.h"
#include "device-control.h"
#include "container-control.h"
#include "container-config.h"

#include <systemd/sd-daemon.h>
#include <systemd/sd-event.h>

static const char TEST_CONF_PATH[] = "test/container-manager-host.json";

static int sigterm_notify(const struct signalfd_siginfo *si, void *userdata)
{
	#ifdef _PRINTF_DEBUG_
	fprintf(stderr,"sigterm_notify\n");
	#endif

	return -1;	//exit event loop
}
static signal_util_t util_array[1] = {
	[0] = {
		.signal = SIGTERM,
		.userdata = NULL,
		.signal_notify = sigterm_notify
	}
};

int main(int argc, char *argv[])
{
	int ret = -1;
	sd_event *event = NULL;
	containers_t *cs = NULL;
	container_control_interface_t *cci = NULL;
	dynamic_device_manager_t *ddm = NULL;
	struct lxc_container *plxc = NULL;
	block_device_info_t bdi;

	ret = sd_event_default(&event);
	if (ret < 0)
		goto finish;

	ret = container_mngsm_setup(&cs, event, NULL);
	if (ret < 0) {
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr,"container_mngsm_setup: fail %d\n", ret);
		#endif
		goto finish;
	}

	ret = container_mngsm_interface_get(&cci, cs);
	if (ret < 0) {
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr,"container_mngsm_interface_create: fail %d\n", ret);
		#endif
		goto finish;
	}

	ret = devc_device_manager_setup(&ddm, cci, event);
	if (ret < 0) {
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr,"devc_device_manager_setup: fail %d\n", ret);
		#endif
		goto finish;
	}

	ret = container_mngsm_regist_device_manager(cs, ddm);
	if (ret < 0) {
		#ifdef _PRINTF_DEBUG_
		fprintf(stderr,"container_mngsm_regist_device_manager: fail %d\n", ret);
		#endif
		goto finish;
	}

	// early device setup: commn time setup all containers, for static device, gpio, 
	ret = devc_early_device_setup(cs);
	if (ret < 0)
		goto finish;

	ret = signal_setup(event, util_array, 1);
	if (ret < 0)
		goto finish;

	// Enable automatic service watchdog support
	ret = sd_event_set_watchdog(event, 1);
	if (ret < 0)
		goto finish;

	//ret = lxc_container_put(plxc);
	ret = container_start(cs);

	(void) sd_notify(
		1,
		"READY=1\n"
		"STATUS=Daemon startup completed, processing events.");

	ret = sd_event_loop(event);

finish:
	if (cs != NULL) {
		(void) container_terminate(cs);
		(void) container_mngsm_cleanup(cs);
	}

	(void) devc_device_manager_cleanup(ddm);

	event = sd_event_unref(event);

	return 0;;
}
