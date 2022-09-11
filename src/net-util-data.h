/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	net-util-data.h
 * @brief	utility header
 */
#ifndef NET_UTIL_DATA_H
#define NET_UTIL_DATA_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <net/if.h>

//-----------------------------------------------------------------------------
typedef struct s_netifinfo {
	int ifindex;
	char ifname[IFNAMSIZ+1];
} netifinfo_t;

#define NETIFINFO_TABLE_MAX (64)
typedef struct s_netifinfo_table {
	int num;
	netifinfo_t tbl[NETIFINFO_TABLE_MAX];
} netifinfo_table_t;


//-----------------------------------------------------------------------------
#endif //#ifndef NET_UTIL_DATA_H
