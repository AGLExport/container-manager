/* libsocketcangw.h
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2025 Automotive Grade Linux
 **/

#ifndef _socketcangw_h
#define _socketcangw_h

/**
 * @file
 * @brief API overview
 */
#include <linux/can.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


#define SOCKETCAN_GW_DIRECTION_IN (0x00000000U)
#define SOCKETCAN_GW_DIRECTION_OUT (0x00000001U)

struct s_socketcan_gw_rule {
	unsigned int src_ifindex;
	unsigned int dst_ifindex;

	unsigned int echo;
	unsigned int invert_can_id;
	unsigned int direction;
	struct can_filter filter;
};
typedef struct s_socketcan_gw_rule socketcan_gw_rule_t;

struct s_socketcan_gw_rules {
	size_t rule_num;
	socketcan_gw_rule_t **rules;
	// internal use
	size_t array_num;
};
typedef struct s_socketcan_gw_rules socketcan_gw_rules_t;

int cangw_add_rule(socketcan_gw_rule_t *rule);
int cangw_add_rules(socketcan_gw_rules_t *gw_rules);
int cangw_delete_rule(socketcan_gw_rule_t *rule);
int cangw_delete_rules(socketcan_gw_rules_t *gw_rules);
int cangw_clean_rule(void);
int cangw_get_rules(socketcan_gw_rules_t **gw_rules);
int cangw_push_gw_rule(socketcan_gw_rules_t *gw_rules, const socketcan_gw_rule_t *rule);
int cangw_pop_gw_rule(socketcan_gw_rules_t *gw_rules, const socketcan_gw_rule_t *rule);
int cangw_release_rules(socketcan_gw_rules_t *gw_rules);

#ifdef __cplusplus
}
#endif

#endif