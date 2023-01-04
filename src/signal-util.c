/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	signal-util.c
 * @brief	This file include UNIX signal utility functions for container manager.
 */
#include "signal-util.h"

#include <stdlib.h>
#include <systemd/sd-event.h>

#include <stdio.h>

#define FAKE_SIGRTMAX	(128)
typedef struct s_signal_util_manage {
	signal_util_t  signal_behavior_table[FAKE_SIGRTMAX+1];
	sd_event *event;
} signal_util_manage_t;

static signal_util_manage_t g_sigutil_mng;

/**
 * Sub function for UNIX signal handling.
 * Block SIGTERM, when this process receive SIGTERM, event loop will exit.
 *
 * @param [in]	event	Instance of sd_event
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
static int sd_event_signal_handler(sd_event_source *s, const struct signalfd_siginfo *si, void *userdata)
{
	signal_util_manage_t* sigutil_mng = (signal_util_manage_t*)userdata;
	int signalnum = -1;
	int ret = -1;

	if (sigutil_mng == NULL) {
		return -1;
	}

	signalnum = sd_event_source_get_signal(s);

	if (0 < signalnum && signalnum <= FAKE_SIGRTMAX) {
		signal_util_t  *elem = &sigutil_mng->signal_behavior_table[signalnum];

		if (elem->signal == signalnum) {

			if (elem->signal_notify != NULL){

				ret = elem->signal_notify(si, elem->userdata);
			}
		}

		if (ret < 0) {
			(void) sd_event_exit(sigutil_mng->event, -1);
			#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
			fprintf(stderr,"[CM CRITICAL ERROR] sd_event_signal_handler notification fail. force exit event loop.\n");
			#endif
		}
	}

	return 0;
}
/**
 * Sub function for UNIX signal handling.
 * Block SIGTERM, when this process receive SIGTERM, event loop will exit.
 *
 * @param [in]	event	Instance of sd_event
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
int signal_setup(sd_event *event, signal_util_t *util_array, int array_num)
{
	sigset_t ss;
	int ret = -1;

	if (event == NULL || util_array == NULL || array_num < 1)
		return -2;

	// If the correct arguments are given, sigemptyset and sigaddset function will never fail.
	(void) sigemptyset(&ss);

	for(int i=0; i < array_num; i++) {
		int signalnum = util_array[i].signal;

		if (0 < signalnum && signalnum <= FAKE_SIGRTMAX) {
			(void) sigaddset(&ss, signalnum);
		}
	}

	// Block signals
	ret = pthread_sigmask(SIG_BLOCK, &ss, NULL);
	if (ret < 0)
		goto err_return;

	for(int i=0; i < array_num; i++) {
		int signalnum = util_array[i].signal;

		if (0 < signalnum && signalnum <= FAKE_SIGRTMAX) {
			g_sigutil_mng.signal_behavior_table[signalnum].signal = signalnum;
			g_sigutil_mng.signal_behavior_table[signalnum].userdata = util_array[i].userdata;
			g_sigutil_mng.signal_behavior_table[signalnum].signal_notify = util_array[i].signal_notify;

			ret = sd_event_add_signal(event, NULL, signalnum, sd_event_signal_handler, &g_sigutil_mng);
			if (ret < 0) {
				goto err_return;
			}
		}
	}

	g_sigutil_mng.event = event;

	return 0;

err_return:
	pthread_sigmask(SIG_UNBLOCK, &ss, NULL);

	return -1;
}
