/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	net-util-data.h
 * @brief	The header for network utility common data.
 */
#ifndef NET_UTIL_DATA_H
#define NET_UTIL_DATA_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <net/if.h>

//-----------------------------------------------------------------------------
/**
 * @typedef	netifinfo_t
 * @brief	Typedef for struct s_netifinfo.
 */
/**
 * @struct	s_netifinfo
 * @brief	Combination table element of network interface between index and name.
 */
typedef struct s_netifinfo {
	int ifindex;				/**< Interface index for this network interface. */
	char ifname[IFNAMSIZ+1];	/**< Interface name for this network interface. */
} netifinfo_t;

/**
 * @def	NETIFINFO_TABLE_MAX
 * @brief	Maximum number of netifinfo_t at s_netifinfo_table.
 */
#define NETIFINFO_TABLE_MAX (64)

/**
 * @typedef	netifinfo_table_t
 * @brief	Typedef for struct s_netifinfo_table.
 */
/**
 * @struct	s_netifinfo_table
 * @brief	Combination table of network interface between index and name.
 */
typedef struct s_netifinfo_table {
	int num;								/**< Num of available network interface. */
	netifinfo_t tbl[NETIFINFO_TABLE_MAX];
} netifinfo_table_t;

//-----------------------------------------------------------------------------
#endif //#ifndef NET_UTIL_DATA_H
