/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	signal-util.h
 * @brief	signal utility header
 */
#ifndef SIGNAL_UTIL_H
#define SIGNAL_UTIL_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <systemd/sd-event.h>


typedef struct s_signal_util {
	int signal;
	void *userdata;
	int (*signal_notify)(const struct signalfd_siginfo *si, void *userdata);
} signal_util_t;

int signal_setup(sd_event *event, signal_util_t *util_array, int array_num);

//-----------------------------------------------------------------------------
#endif //#ifndef SIGNAL_UTIL_H
