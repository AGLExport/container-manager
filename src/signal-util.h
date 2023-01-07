/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	signal-util.h
 * @brief	The header for signal utility.
 */
#ifndef SIGNAL_UTIL_H
#define SIGNAL_UTIL_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <systemd/sd-event.h>

/**
 * @typedef	signal_util_t
 * @brief	Typedef for struct s_signal_util.
 */
/**
 * @struct	s_signal_util
 * @brief	The structure of signal handling information to set handling callback and user data to signal.
 */
typedef struct s_signal_util {
	int signal;			/**< UNIX signal number. */
	void *userdata;		/**< Pointer to any user data. */
	int (*signal_notify)(const struct signalfd_siginfo *si, void *userdata);	/**< Callback function for selected signal handling. */
} signal_util_t;

int signal_setup(sd_event *event, signal_util_t *util_array, int array_num);

//-----------------------------------------------------------------------------
#endif //#ifndef SIGNAL_UTIL_H
