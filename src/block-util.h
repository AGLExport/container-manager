/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	block-util.h
 * @brief	block devide utility header
 */
#ifndef BLOCK_UTIL_H
#define BLOCK_UTIL_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>


//-----------------------------------------------------------------------------
typedef struct s_block_device_info {
    uint32_t    fsmagic;
    char volume_label[32];
} block_device_info_t;

int block_util_getfs(const char *devpath, block_device_info_t *bdi);
//-----------------------------------------------------------------------------
#endif //#ifndef BLOCK_UTIL_H