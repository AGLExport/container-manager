/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	util.c
 * @brief	utility functions
 */
#include "signal-util.h"

#include <stdlib.h>
#include <systemd/sd-event.h>

#include <stdio.h>



/**
 * Sub function for UNIX signal handling.
 * Block SIGTERM, when this process receive SIGTERM, event loop will exit.
 *
 * @param [in]	event	Incetance of sd_event
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
static int sd_event_sigchld_handler(sd_event_source *s, const struct signalfd_siginfo *si, void *userdata)
{

	fprintf(stderr,"signal %d from %d\n", si->ssi_signo, si->ssi_pid);

	return 0;
}
/**
 * Sub function for UNIX signal handling.
 * Block SIGTERM, when this process receive SIGTERM, event loop will exit.
 *
 * @param [in]	event	Incetance of sd_event
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
static int sd_event_sigterm_handler(sd_event_source *s, const struct signalfd_siginfo *si, void *userdata)
{
	sd_event *event = (sd_event*)userdata;

	fprintf(stderr,"signal %d from %d\n", si->ssi_signo, si->ssi_pid);

	(void) sd_event_exit(event, 0);

	return 0;
}
/**
 * Sub function for UNIX signal handling.
 * Block SIGTERM, when this process receive SIGTERM, event loop will exit.
 *
 * @param [in]	event	Incetance of sd_event
 * @return int	 0 success
 * 				-2 argument error
 *				-1 internal error
 */
int signal_setup(sd_event *event, signal_util_t *util_array, int array_num)
{
	sigset_t ss;
	int ret = -1;

	if (event == NULL)
		return -2;

	// If the correct arguments are given, these function will never fail.
	(void) sigemptyset(&ss);
	(void) sigaddset(&ss, SIGTERM);
	(void) sigaddset(&ss, SIGCHLD);

	// Block SIGTERM
	ret = pthread_sigmask(SIG_BLOCK, &ss, NULL);
	if (ret < 0)
		goto err_return;

	// Block SIGCHLD
	ret = pthread_sigmask(SIGCHLD, &ss, NULL);
	if (ret < 0)
		goto err_return;

	ret = sd_event_add_signal(event, NULL, SIGTERM, sd_event_sigterm_handler, event);
	if (ret < 0) {
		goto err_return;
	}
	ret = sd_event_add_signal(event, NULL, SIGCHLD, sd_event_sigchld_handler, NULL);
	if (ret < 0) {
		goto err_return;
	}

	return 0;

err_return:
	pthread_sigmask(SIG_UNBLOCK, &ss, NULL);

	return -1;
}
