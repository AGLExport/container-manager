/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	interface_test.cpp
 * @brief	Public interface test fot refop
 */
#include <gtest/gtest.h>

#include <string.h>

// Test Terget files ---------------------------------------
extern "C" {
#include "../../../src/parser/parser-common.c"
#include "../../../src/parser/parser-container.c"
}
// Test Terget files ---------------------------------------
using namespace ::testing;

struct parser_test : Test {};

// Stub ---------------------------------------
extern "C" {
	static int fprintf(FILE *stream, const char *format, ...) { return 0;}
}
//--------------------------------------------------------------------------------------------------------
TEST_F(parser_test, cmparser_create_from_file__argerr)
{
	int ret = -1;
	container_config_t *cc = NULL;

	ret = cmparser_create_from_file(NULL, NULL);
	ASSERT_EQ(-1, ret);

	ret = cmparser_create_from_file(&cc, NULL);
	ASSERT_EQ(-1, ret);

	ret = cmparser_create_from_file(NULL, "test");
	ASSERT_EQ(-1, ret);
}
//--------------------------------------------------------------------------------------------------------
TEST_F(parser_test, cmparser_create_from_file__nofileerr)
{
	int ret = -1;
	container_config_t *cc = NULL;

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-nofile-00.json");
	ASSERT_EQ(-1, ret);

	cmparser_release_config(cc);
}
//--------------------------------------------------------------------------------------------------------
TEST_F(parser_test, cmparser_create_from_file__nodataerr)
{
	int ret = -1;
	container_config_t *cc = NULL;

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-nodata-00.json");
	ASSERT_EQ(-2, ret);

	cmparser_release_config(cc);
}
//--------------------------------------------------------------------------------------------------------
TEST_F(parser_test, cmparser_create_from_file__nonameerr)
{
	int ret = -1;
	container_config_t *cc = NULL;

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-noname-00.json");
	ASSERT_EQ(-2, ret);

	cmparser_release_config(cc);
}
//--------------------------------------------------------------------------------------------------------
TEST_F(parser_test, cmparser_create_from_file__nameonlyerr)
{
	int ret = -1;
	container_config_t *cc = NULL;

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-nameonly-00.json");
	ASSERT_EQ(-2, ret);

	cmparser_release_config(cc);
}
//--------------------------------------------------------------------------------------------------------
TEST_F(parser_test, cmparser_create_from_file__base00_autoboot)
{
	int ret = -1;
	container_config_t *cc = NULL;

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/00/test-base-00-0.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/00/test-base-00-1.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/00/test-base-00-2.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/00/test-base-00-3.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);
}
//--------------------------------------------------------------------------------------------------------
TEST_F(parser_test, cmparser_create_from_file__base01_bootpriority)
{
	int ret = -1;
	container_config_t *cc = NULL;

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/01/test-base-01-0.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/01/test-base-01-1.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/01/test-base-01-2.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);
}
//--------------------------------------------------------------------------------------------------------
TEST_F(parser_test, cmparser_create_from_file__base02_rootfs)
{
	int ret = -1;
	container_config_t *cc = NULL;

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/02/test-base-02-0.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/02/test-base-02-1.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/02/test-base-02-2.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/02/test-base-02-3.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/02/test-base-02-4.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/02/test-base-02-5.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/02/test-base-02-6.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/02/test-base-02-7.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/02/test-base-02-8.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/02/test-base-02-9.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base/02/test-base-02-10.json");
	ASSERT_EQ(-2, ret);
	ASSERT_EQ(NULL, cc);

	cmparser_release_config(cc);
}
//--------------------------------------------------------------------------------------------------------
#if 0
TEST_F(parser_test, cmparser_create_from_file__argerr)
{
	int ret = -1;
	container_config_t *cc = NULL;

	ret = cmparser_create_from_file(&cc, "test/unit/data/test-base-00.json");
	ASSERT_EQ(-1, ret);
}
#endif

#if  0
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
#endif
