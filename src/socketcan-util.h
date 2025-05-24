/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	socketcan-util.h
 * @brief	The header for socketcan utility.
 */
#ifndef SOCKETCAN_UTIL_H
#define SOCKETCAN_UTIL_H
//-----------------------------------------------------------------------------
#include <stdint.h>

//-----------------------------------------------------------------------------
int socketcanutil_create_vxcan_peer(const char *ifname, const char *peer_ifname);
int socketcanutil_up_can_if(const char *ifname);
int socketcanutil_remove_vxcan_peer(const char *ifname);
int socketcanutil_configure_gateway(const char *src_ifname, const char *dest_ifname);
//-----------------------------------------------------------------------------
#endif //#ifndef SOCKETCAN_UTIL_H
