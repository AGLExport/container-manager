/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	interface_test.cpp
 * @brief	Public interface test fot refop
 */
#include <gtest/gtest.h>

// Test Terget files ---------------------------------------
extern "C" {
#include "../lib/libredundancyfileop.c"
}
// Test Terget files ---------------------------------------
using namespace ::testing;

struct interface_test : Test {};

//dummy data
static const char directry[] = "/tmp/refop-test/";
static const char file[] = "test.bin";
static const char newfile[] = "/tmp/refop-test/test.bin.tmp";
static const char latestfile[] = "/tmp/refop-test/test.bin";
static const char backupfile[] = "/tmp/refop-test/test.bin.bk1";

//--------------------------------------------------------------------------------------------------------
// Interface test for data set with some data pattern.
TEST_F(interface_test, interface_test_refop_set_redundancy_data__success)
{
	struct refop_halndle *hndl;
	refop_error_t ret = REFOP_SUCCESS;
	refop_handle_t handle = NULL;

	//dummy data
	uint8_t *pbuf = NULL;
	int64_t sz = 1 * 1024 * 1024;

	//clean up
	(void)mkdir(directry, 0777);
	(void)unlink(newfile);
	(void)unlink(latestfile);
	(void)unlink(backupfile);

	pbuf = (uint8_t*)malloc(sz);

	ret = refop_create_redundancy_handle(&handle, directry, file);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0xff,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0xa5,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0x5a,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	ret = refop_release_redundancy_handle(handle);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	free(pbuf);
}
//--------------------------------------------------------------------------------------------------------
// Interface test for data get.
TEST_F(interface_test, interface_test_refop_get_redundancy_data__success)
{
	struct refop_halndle *hndl;
	refop_error_t ret = REFOP_SUCCESS;
	refop_handle_t handle = NULL;

	//dummy data
	uint8_t *pbuf = NULL;
	int64_t sz = 1 * 1024 * 1024;
	int64_t szr = 0;

	//clean up
	(void)mkdir(directry, 0777);
	(void)unlink(newfile);
	(void)unlink(latestfile);
	(void)unlink(backupfile);

	pbuf = (uint8_t*)malloc(sz);

	ret = refop_create_redundancy_handle(&handle, directry, file);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	ret = refop_get_redundancy_data(handle, pbuf, sz, &szr);
	ASSERT_EQ(REFOP_NOENT, ret);

	memset(pbuf,0xff,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	ret = refop_get_redundancy_data(handle, pbuf, sz, &szr);
	ASSERT_EQ(REFOP_SUCCESS, ret);
	ASSERT_EQ(sz, szr);

	memset(pbuf,0x00,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	ret = refop_get_redundancy_data(handle, pbuf, sz, &szr);
	ASSERT_EQ(REFOP_SUCCESS, ret);
	ASSERT_EQ(sz, szr);

	ret = refop_release_redundancy_handle(handle);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	free(pbuf);
}
//--------------------------------------------------------------------------------------------------------
// Interface test for data rotate.
TEST_F(interface_test, interface_test_refop_set_and_get)
{
	struct refop_halndle *hndl;
	refop_error_t ret = REFOP_SUCCESS;
	refop_handle_t handle = NULL;

	//dummy data
	char directry[] = "/tmp/refop-test/";
	char file[] = "test.bin";
	char newfile[] = "/tmp/refop-test/test.bin.tmp";
	char latestfile[] = "/tmp/refop-test/test.bin";
	char backupfile[] = "/tmp/refop-test/test.bin.bk1";

	uint8_t *pbuf = NULL;
	uint8_t *prbuf = NULL;
	int64_t sz = 64 * 1024;
	int64_t szr = 0;
	int checker = 0;

	//clean up
	(void)mkdir(directry, 0777);
	(void)unlink(newfile);
	(void)unlink(latestfile);
	(void)unlink(backupfile);

	pbuf = (uint8_t*)malloc(sz);
	prbuf = (uint8_t*)malloc(sz);

	ret = refop_create_redundancy_handle(&handle, directry, file);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0xff,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	// Operation algorithm
	//     Current                 Next
	//    | latest | backup |     | latest | backup |
	// a1 |    1   |   2    |     |  new   |    1   |
	// a2 |    1   |   x    |     |  new   |    1   |
	// a3 |    x   |   2    |     |  new   |    2   |
	// a4 |    x   |   x    |     |  new   |    x   |

	// a1
	ret = refop_get_redundancy_data(handle, prbuf, sz, &szr);
	ASSERT_EQ(REFOP_SUCCESS, ret);
	ASSERT_EQ(sz, szr);
	for(int i=0;i < szr;i++) {
		checker += (prbuf[i] - (uint8_t)0xff);
	}
	ASSERT_EQ(0, checker);

	memset(pbuf,0xa1,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);


	// a2
	unlink(backupfile);
	ret = refop_get_redundancy_data(handle, prbuf, sz, &szr);
	ASSERT_EQ(REFOP_SUCCESS, ret);
	ASSERT_EQ(sz, szr);
	for(int i=0;i < szr;i++) {
		checker += (prbuf[i] - (uint8_t)0xa1);
	}
	ASSERT_EQ(0, checker);

	memset(pbuf,0xa2,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	// a3
	unlink(latestfile);
	ret = refop_get_redundancy_data(handle, prbuf, sz, &szr);
	ASSERT_EQ(REFOP_RECOVER, ret);
	ASSERT_EQ(sz, szr);
	for(int i=0;i < szr;i++) {
		checker += (prbuf[i] - (uint8_t)0xa1);
	}
	ASSERT_EQ(0, checker);

	memset(pbuf,0xa3,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	// a4
	unlink(backupfile);
	unlink(latestfile);
	ret = refop_get_redundancy_data(handle, prbuf, sz, &szr);
	ASSERT_EQ(REFOP_NOENT, ret);

	memset(pbuf,0xa4,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	ret = refop_remove_redundancy_data(handle);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	ret = refop_release_redundancy_handle(handle);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	free(pbuf);
	free(prbuf);
}
//--------------------------------------------------------------------------------------------------------
// Interface test for data rotate with small data read.
TEST_F(interface_test, interface_test_refop_set_and_get_smallread)
{
	struct refop_halndle *hndl;
	refop_error_t ret = REFOP_SUCCESS;
	refop_handle_t handle = NULL;

	//dummy data
	char directry[] = "/tmp/refop-test/";
	char file[] = "test.bin";
	char newfile[] = "/tmp/refop-test/test.bin.tmp";
	char latestfile[] = "/tmp/refop-test/test.bin";
	char backupfile[] = "/tmp/refop-test/test.bin.bk1";

	uint8_t *pbuf = NULL;
	uint8_t *prbuf = NULL;
	int64_t sz = 4 * 1024;
	int64_t szr = 0;
	int checker = 0;

	//clean up
	(void)mkdir(directry, 0777);
	(void)unlink(newfile);
	(void)unlink(latestfile);
	(void)unlink(backupfile);

	pbuf = (uint8_t*)malloc(sz);
	prbuf = (uint8_t*)malloc(sz);

	ret = refop_create_redundancy_handle(&handle, directry, file);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0xff,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	// Operation algorithm
	//     Current                 Next
	//    | latest | backup |     | latest | backup |
	// a1 |    1   |   2    |     |  new   |    1   |
	// a2 |    1   |   x    |     |  new   |    1   |
	// a3 |    x   |   2    |     |  new   |    2   |
	// a4 |    x   |   x    |     |  new   |    x   |

	// a1
	ret = refop_get_redundancy_data(handle, prbuf, sz/2, &szr);
	ASSERT_EQ(REFOP_SUCCESS, ret);
	ASSERT_EQ(sz/2, szr);
	for(int i=0;i < szr;i++) {
		checker += (prbuf[i] - (uint8_t)0xff);
	}
	ASSERT_EQ(0, checker);

	memset(pbuf,0xa1,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	// a2
	unlink(backupfile);
	ret = refop_get_redundancy_data(handle, prbuf, sz/2, &szr);
	ASSERT_EQ(REFOP_SUCCESS, ret);
	ASSERT_EQ(sz/2, szr);
	for(int i=0;i < szr;i++) {
		checker += (prbuf[i] - (uint8_t)0xa1);
	}
	ASSERT_EQ(0, checker);

	memset(pbuf,0xa2,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	// a3
	unlink(latestfile);
	ret = refop_get_redundancy_data(handle, prbuf, sz/2, &szr);
	ASSERT_EQ(REFOP_RECOVER, ret);
	ASSERT_EQ(sz/2, szr);
	for(int i=0;i < szr;i++) {
		checker += (prbuf[i] - (uint8_t)0xa1);
	}
	ASSERT_EQ(0, checker);

	memset(pbuf,0xa3,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	// a4
	unlink(backupfile);
	unlink(latestfile);
	ret = refop_get_redundancy_data(handle, prbuf, sz/2, &szr);
	ASSERT_EQ(REFOP_NOENT, ret);

	memset(pbuf,0xa4,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	ret = refop_remove_redundancy_data(handle);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	ret = refop_release_redundancy_handle(handle);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	free(pbuf);
	free(prbuf);
}
//--------------------------------------------------------------------------------------------------------
// Interface test for data rotate with large data read.
TEST_F(interface_test, interface_test_refop_set_and_get_largeread)
{
	struct refop_halndle *hndl;
	refop_error_t ret = REFOP_SUCCESS;
	refop_handle_t handle = NULL;

	//dummy data
	char directry[] = "/tmp/refop-test/";
	char file[] = "test.bin";
	char newfile[] = "/tmp/refop-test/test.bin.tmp";
	char latestfile[] = "/tmp/refop-test/test.bin";
	char backupfile[] = "/tmp/refop-test/test.bin.bk1";

	uint8_t *pbuf = NULL;
	uint8_t *prbuf = NULL;
	int64_t sz = 256 * 1024;
	int64_t szr = 0;
	int checker = 0;

	//clean up
	(void)mkdir(directry, 0777);
	(void)unlink(newfile);
	(void)unlink(latestfile);
	(void)unlink(backupfile);

	pbuf = (uint8_t*)malloc(sz);
	prbuf = (uint8_t*)malloc(sz);

	ret = refop_create_redundancy_handle(&handle, directry, file);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0xff,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	// Operation algorithm
	//     Current                 Next
	//    | latest | backup |     | latest | backup |
	// a1 |    1   |   2    |     |  new   |    1   |
	// a2 |    1   |   x    |     |  new   |    1   |
	// a3 |    x   |   2    |     |  new   |    2   |
	// a4 |    x   |   x    |     |  new   |    x   |

	// a1
	ret = refop_get_redundancy_data(handle, prbuf, sz*2, &szr);
	ASSERT_EQ(REFOP_SUCCESS, ret);
	ASSERT_EQ(sz, szr);
	for(int i=0;i < szr;i++) {
		checker += (prbuf[i] - (uint8_t)0xff);
	}
	ASSERT_EQ(0, checker);

	memset(pbuf,0xa1,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	// a2
	unlink(backupfile);
	ret = refop_get_redundancy_data(handle, prbuf, sz*2, &szr);
	ASSERT_EQ(REFOP_SUCCESS, ret);
	ASSERT_EQ(sz, szr);
	for(int i=0;i < szr;i++) {
		checker += (prbuf[i] - (uint8_t)0xa1);
	}
	ASSERT_EQ(0, checker);

	memset(pbuf,0xa2,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	// a3
	unlink(latestfile);
	ret = refop_get_redundancy_data(handle, prbuf, sz*2, &szr);
	ASSERT_EQ(REFOP_RECOVER, ret);
	ASSERT_EQ(sz, szr);
	for(int i=0;i < szr;i++) {
		checker += (prbuf[i] - (uint8_t)0xa1);
	}
	ASSERT_EQ(0, checker);

	memset(pbuf,0xa3,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	// a4
	unlink(backupfile);
	unlink(latestfile);
	ret = refop_get_redundancy_data(handle, prbuf, sz*2, &szr);
	ASSERT_EQ(REFOP_NOENT, ret);

	memset(pbuf,0xa4,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	ret = refop_remove_redundancy_data(handle);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	ret = refop_release_redundancy_handle(handle);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	free(pbuf);
	free(prbuf);
}
//--------------------------------------------------------------------------------------------------------
// Interface test for data pick up with small data read.
TEST_F(interface_test, interface_test_no_file_f1)
{
	struct refop_halndle *hndl;
	refop_error_t ret = REFOP_SUCCESS;
	refop_handle_t handle = NULL;

	//dummy data
	uint8_t *pbuf = NULL;
	uint8_t *prbuf = NULL;
	int64_t sz = 4 * 1024;
	int64_t szr = 0;
	int checker = 0;

	//clean up
	(void)mkdir(directry, 0777);
	(void)unlink(newfile);
	(void)unlink(latestfile);
	(void)unlink(backupfile);

	pbuf = (uint8_t*)malloc(sz);
	prbuf = (uint8_t*)malloc(sz);

	ret = refop_create_redundancy_handle(&handle, directry, file);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0xff,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	// file pattern
	//    | latest | backup |
	// f1 |    o   |   o    |
	// f2 |    o   |   -    |
	// f3 |    -   |   o    |
	// f4 |    -   |   -    |
	
	// f1
	ret = refop_get_redundancy_data(handle, prbuf, sz, &szr);
	ASSERT_EQ(REFOP_SUCCESS, ret);
	ASSERT_EQ(sz, szr);
	for(int i=0;i < szr;i++) {
		checker += (prbuf[i] - (uint8_t)0xff);
	}
	ASSERT_EQ(0, checker);

	ret = refop_remove_redundancy_data(handle);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	ret = refop_release_redundancy_handle(handle);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	free(pbuf);
	free(prbuf);
}
//--------------------------------------------------------------------------------------------------------
// Interface test for data pick up with small data read.
TEST_F(interface_test, interface_test_no_file_f2)
{
	struct refop_halndle *hndl;
	refop_error_t ret = REFOP_SUCCESS;
	refop_handle_t handle = NULL;

	//dummy data
	uint8_t *pbuf = NULL;
	uint8_t *prbuf = NULL;
	int64_t sz = 4 * 1024;
	int64_t szr = 0;
	int checker = 0;

	//clean up
	(void)mkdir(directry, 0777);
	(void)unlink(newfile);
	(void)unlink(latestfile);
	(void)unlink(backupfile);

	pbuf = (uint8_t*)malloc(sz);
	prbuf = (uint8_t*)malloc(sz);

	//short directry string
	ret = refop_create_redundancy_handle(&handle, directry, file);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0xff,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	// file pattern
	//    | latest | backup |
	// f1 |    o   |   o    |
	// f2 |    o   |   -    |
	// f3 |    -   |   o    |
	// f4 |    -   |   -    |

	(void)unlink(backupfile);

	// f2
	ret = refop_get_redundancy_data(handle, prbuf, sz, &szr);
	ASSERT_EQ(REFOP_SUCCESS, ret);
	ASSERT_EQ(sz, szr);
	for(int i=0;i < szr;i++) {
		checker += (prbuf[i] - (uint8_t)0xff);
	}
	ASSERT_EQ(0, checker);

	ret = refop_remove_redundancy_data(handle);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	ret = refop_release_redundancy_handle(handle);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	free(pbuf);
	free(prbuf);
}
//--------------------------------------------------------------------------------------------------------
// Interface test for data pick up with small data read.
TEST_F(interface_test, interface_test_no_file_f3)
{
	struct refop_halndle *hndl;
	refop_error_t ret = REFOP_SUCCESS;
	refop_handle_t handle = NULL;

	//dummy data
	uint8_t *pbuf = NULL;
	uint8_t *prbuf = NULL;
	int64_t sz = 4 * 1024;
	int64_t szr = 0;
	int checker = 0;

	//clean up
	(void)mkdir(directry, 0777);
	(void)unlink(newfile);
	(void)unlink(latestfile);
	(void)unlink(backupfile);

	pbuf = (uint8_t*)malloc(sz);
	prbuf = (uint8_t*)malloc(sz);

	//short directry string
	ret = refop_create_redundancy_handle(&handle, directry, file);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0xff,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	// file pattern
	//    | latest | backup |
	// f1 |    o   |   o    |
	// f2 |    o   |   -    |
	// f3 |    -   |   o    |
	// f4 |    -   |   -    |

	(void)unlink(latestfile);

	// f3
	ret = refop_get_redundancy_data(handle, prbuf, sz, &szr);
	ASSERT_EQ(REFOP_RECOVER, ret);
	ASSERT_EQ(sz, szr);
	for(int i=0;i < szr;i++) {
		checker += (prbuf[i] - (uint8_t)0);
	}
	ASSERT_EQ(0, checker);

	ret = refop_remove_redundancy_data(handle);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	ret = refop_release_redundancy_handle(handle);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	free(pbuf);
	free(prbuf);
}
//--------------------------------------------------------------------------------------------------------
// Interface test for data pick up with small data read.
TEST_F(interface_test, interface_test_no_file_f4)
{
	struct refop_halndle *hndl;
	refop_error_t ret = REFOP_SUCCESS;
	refop_handle_t handle = NULL;

	//dummy data
	uint8_t *pbuf = NULL;
	uint8_t *prbuf = NULL;
	int64_t sz = 4 * 1024;
	int64_t szr = 0;
	int checker = 0;

	//clean up
	(void)mkdir(directry, 0777);
	(void)unlink(newfile);
	(void)unlink(latestfile);
	(void)unlink(backupfile);

	pbuf = (uint8_t*)malloc(sz);
	prbuf = (uint8_t*)malloc(sz);

	//short directry string
	ret = refop_create_redundancy_handle(&handle, directry, file);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	memset(pbuf,0xff,sz);
	ret = refop_set_redundancy_data(handle, pbuf, sz);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	// file pattern
	//    | latest | backup |
	// f1 |    o   |   o    |
	// f2 |    o   |   -    |
	// f3 |    -   |   o    |
	// f4 |    -   |   -    |

	(void)unlink(latestfile);
	(void)unlink(backupfile);

	// f4
	ret = refop_get_redundancy_data(handle, prbuf, sz, &szr);
	ASSERT_EQ(REFOP_NOENT, ret);

	ret = refop_remove_redundancy_data(handle);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	ret = refop_release_redundancy_handle(handle);
	ASSERT_EQ(REFOP_SUCCESS, ret);

	free(pbuf);
	free(prbuf);
}
