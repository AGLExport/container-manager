/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LIBSOCKETCANGW_CONFIG_H
#define LIBSOCKETCANGW_CONFIG_H

#include <libsocketcangw.h>

#ifdef __cplusplus
extern "C" {
#endif

int cangw_config_load_from_file(socketcan_gw_rules_t **ppsgr, const char *file);
void cangw_config_release_config(socketcan_gw_rules_t *psgr);
int cangw_config_refill_ifindex(socketcan_gw_rules_t *psgr, unsigned int dst_ifindex, unsigned int src_ifindex);
int cangw_config_log_enable(void);
#ifdef __cplusplus
}
#endif
#endif
