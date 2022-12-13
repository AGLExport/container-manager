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
 * Once write util
 *
 * @param [in]	path	File path
 * @param [in]	data	Write data
 * @param [in]	size	Write data size
 * @return int
 * @retval  0 Success.
 * @retval -1 Write error.
 */
int block_util_getfs(const char *devpath, block_device_info_t *bdi)
{
	blkid_probe blk = NULL;
	int ret = -1;
	const char *data;
	size_t sz;
	uint32_t magic = 0;
	uint8_t *pmagic = (uint8_t*)&magic;

	if (devpath == NULL || bdi == NULL)
		return -1;

	blk = blkid_new_probe_from_filename(devpath);
	if (blk == NULL)
		return -1;

	ret = blkid_probe_enable_superblocks(blk, 1);
	if (ret < 0)
		goto error_ret;

	ret = blkid_probe_set_superblocks_flags(blk, BLKID_SUBLKS_LABEL | BLKID_SUBLKS_MAGIC);
	if (ret < 0)
		goto error_ret;

	ret = blkid_do_safeprobe(blk);
	if (ret < 0)
		goto error_ret;

	// fs magic
	ret = blkid_probe_lookup_value(blk, "SBMAGIC", &data, &sz);
	if (ret == 0) {
		if (sz == 4) {
			pmagic[0] = data[0];
			pmagic[1] = data[1];
			pmagic[2] = data[2];
			pmagic[3] = data[3];
		} else if (sz == 2) {
			pmagic[0] = data[0];
			pmagic[1] = data[1];
		} else {
			goto error_ret;
		}
	} else {
		goto error_ret;
	}

	// volume label
	ret = blkid_probe_lookup_value(blk, "LABEL", &data, &sz);
	if (ret == 0 && sz <= 16) {
		// have a vol label
		memcpy(bdi->volume_label, data, sz);
		bdi->volume_label[sz] = '\0';
	} else
		bdi->volume_label[0] = '\0';

	bdi->fsmagic = magic;

	blkid_free_probe(blk);

	#ifdef _PRINTF_DEBUG_
	fprintf(stderr, "%s : magic = %4x, label = %s\n", devpath, bdi->fsmagic, bdi->volume_label);
	#endif

	return 0;

error_ret:
	if (blk != NULL)
		blkid_free_probe(blk);

	return -1;
}
