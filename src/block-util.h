/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	block-util.h
 * @brief	block device utility header
 */
#ifndef BLOCK_UTIL_H
#define BLOCK_UTIL_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>

//-----------------------------------------------------------------------------
/**
 * @typedef	block_device_info_t
 * @brief	Typedef for struct s_block_device_info.
 */
/**
 * @struct	s_block_device_info
 * @brief	The data structure for block device information, that use in container manager.
 */
typedef struct s_block_device_info {
    char type[32];
    char volume_label[32];  /**< Volume label for probed device. */
} block_device_info_t;

int block_util_getfs(const char *devpath, block_device_info_t *bdi);
//-----------------------------------------------------------------------------
#endif //#ifndef BLOCK_UTIL_H
