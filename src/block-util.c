/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	block-util.c
 * @brief	block device utility functions
 */
#undef _PRINTF_DEBUG_

#include "block-util.h"

#include <blkid/blkid.h>
#include <stdio.h>
#include <string.h>

/**
 * The function of filesystem scan for block device.
 * This function support filesystem type(magic) and volume label probing.
 *
 * @param [in]	devpath	Device node path.  Ex. "/dev/sda1"
 * @param [out]	bdi		Pointer to block_device_info_t
 * @return int
 * @retval  0 Success.
 * @retval -1 No or not probing support file system.
 */
int block_util_getfs(const char *devpath, block_device_info_t *bdi)
{
	blkid_probe blk = NULL;
	int ret = -1;
	const char *data;
	size_t sz;

	if (devpath == NULL || bdi == NULL) {
		return -1;
	}

	blk = blkid_new_probe_from_filename(devpath);
	if (blk == NULL) {
		return -1;
	}

	ret = blkid_probe_enable_superblocks(blk, 1);
	if (ret < 0) {
		goto error_ret;
	}

	ret = blkid_probe_set_superblocks_flags(blk, BLKID_SUBLKS_LABEL | BLKID_SUBLKS_TYPE);
	if (ret < 0) {
		goto error_ret;
	}

	ret = blkid_do_safeprobe(blk);
	if (ret < 0) {
		goto error_ret;
	}

	// fs type
	ret = blkid_probe_lookup_value(blk, "TYPE", &data, &sz);
	if (ret == 0 && sz <= 31u) {
		// have a vol label
		(void) memcpy(bdi->type, data, sz);
		bdi->type[sz] = '\0';
	} else {
		bdi->type[0] = '\0';
	}

	// volume label
	ret = blkid_probe_lookup_value(blk, "LABEL", &data, &sz);
	if (ret == 0 && sz <= 16u) {
		// have a vol label
		(void) memcpy(bdi->volume_label, data, sz);
		bdi->volume_label[sz] = '\0';
	} else {
		bdi->volume_label[0] = '\0';
	}

	blkid_free_probe(blk);

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout, "%s : type = %s, label = %s\n", devpath, bdi->type, bdi->volume_label);
	#endif

	return 0;

error_ret:
	if (blk != NULL) {
		blkid_free_probe(blk);
	}

	return -1;
}
