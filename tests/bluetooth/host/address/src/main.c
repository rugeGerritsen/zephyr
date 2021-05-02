/*
 * Copyright (c) 2015 Wind River Systems, Inc.
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>
#include <stdbool.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/addr.h>

#include "mocked_controller.h"

static const bt_addr_le_t peer_addr =
{
	.type = BT_ADDR_LE_RANDOM,
	.a = {.val = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6}}
};

static bt_addr_le_t ids[CONFIG_BT_ID_MAX];

static bool can_start_advertiser(bool adv_use_identity, bool directed)
{
	if (adv_use_identity) {
		if (!IS_ENABLED(CONFIG_BT_SCAN_WITH_IDENTITY)
			&& controller_scan_enabled()
			&& controller_scan_type_active()) {
			/* Scanner is using private address,
			 * advertiser is using identity. */
			return false;
		}
	} else {
		if (IS_ENABLED(CONFIG_BT_SCAN_WITH_IDENTITY)
			&& controller_scan_enabled()
			&& controller_scan_type_active()) {
			/* Scanner is using identity,
			 * advertiser is using private address. */
			return false;
		}
	}

	return true;
}


static void start_adv(struct bt_le_adv_param *adv_param)
{

	bt_addr_t random_addr;
	bool random_addr_valid;

	bool random_addr_shall_contain_identity =
		(adv_param->options & BT_LE_ADV_OPT_USE_IDENTITY);

	bool expect_success =
			can_start_advertiser(random_addr_shall_contain_identity,
								adv_param.peer != NULL);

	if (expect_success) {
		zassert_equal(bt_le_adv_start(adv_param, NULL, 0, NULL, 0), 0,
				"Advertising start failed");
	} else {
		bool success = bt_le_adv_start(adv_param, NULL, 0, NULL, 0) == 0;
		zassert_false(success,
				"Advertising start succeeded");
		return;
	}

	/* Validate if the controller still contains the correct random
	 * address. */
	mocked_controller_random_addr_get(&random_addr, &random_addr_valid);
	zassert_true(random_addr_valid, "Needs a valid random address");

	bool random_addr_is_identity =
			(memcmp(&random_addr,
				    &ids[BT_ID_DEFAULT].a,
				    sizeof(random_addr)) == 0);

	zassert_equal(random_addr_shall_contain_identity, random_addr_is_identity,
			"Random address contains identity: %d \n"
			"Connectable: %d\n"
			"Scannable: %d\n"
			"Directed: %d\n"
			"Use identity %d\n"
			"Privacy enabled %d",
			random_addr_is_identity,
			(adv_param->options & BT_LE_ADV_OPT_CONNECTABLE) != 0,
			(adv_param->options & BT_LE_ADV_OPT_SCANNABLE) != 0,
			adv_param->peer != NULL,
			(adv_param->options & BT_LE_ADV_OPT_USE_IDENTITY) != 0,
			IS_ENABLED(BT_PRIVACY));

	zassert_equal(bt_le_adv_stop(), 0, "Adv stop failed");
}

static void test_legacy_adv(void)
{
	struct bt_le_adv_param adv_param =
			BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_NONE,
					BT_GAP_ADV_FAST_INT_MIN_2,
					BT_GAP_ADV_FAST_INT_MIN_2,
					NULL);
	bool use_identity[] = {false, true};

	/* Undirected advertising types */
	uint32_t undirected_options[] =
	{
		BT_LE_ADV_OPT_NONE,
		BT_LE_ADV_OPT_CONNECTABLE,
		BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_SCANNABLE,
		BT_LE_ADV_OPT_SCANNABLE,
	};

	for (int i = 0; i < ARRAY_SIZE(undirected_options); i++) {
		for (int j = 0; j < ARRAY_SIZE(use_identity); j++) {
			adv_param.options = undirected_options[i];
			if (use_identity[j]) {
				adv_param.options |= BT_LE_ADV_OPT_USE_IDENTITY;
			}

			start_adv(&adv_param);
		}
	}

	/* Directed advertising types*/
	uint32_t directed_options[] =
	{
		BT_LE_ADV_OPT_CONNECTABLE,
		BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY,
	};

	adv_param.peer = &peer_addr;

	bool dir_rpa[] = {true, false};

	for (int i = 0; i < ARRAY_SIZE(directed_options); i++) {
		for (int j = 0; j < ARRAY_SIZE(use_identity); j++) {
			for (int k = 0; k < ARRAY_SIZE(dir_rpa); k++) {

				adv_param.options = directed_options[i];
				if (use_identity[j]) {
					adv_param.options |= BT_LE_ADV_OPT_USE_IDENTITY;
				}

				if (dir_rpa[k]) {
					adv_param.options |= BT_LE_ADV_OPT_DIR_ADDR_RPA;
				}

				start_adv(&adv_param);
			}
		};
	}
}

void test_legacy_adv_no_scan(void)
{
	test_legacy_adv();
}

void test_legacy_adv_passive_scan(void)
{
	zassert_equal(bt_le_scan_start(BT_LE_SCAN_PASSIVE, NULL), 0,
			"Scan start failed");

	test_legacy_adv();

	zassert_equal(bt_le_scan_stop(), 0, "Scan stop failed");
}

void test_legacy_adv_active_scan(void)
{
	zassert_equal(bt_le_scan_start(BT_LE_SCAN_ACTIVE, NULL), 0,
			"Scan start failed");

	test_legacy_adv();

	zassert_equal(bt_le_scan_stop(), 0, "Scan stop failed");
}

void test_main(void)
{
	zassert_equal(bt_enable(NULL), 0,
				  "bt_enable failed");

	size_t id_count = CONFIG_BT_ID_MAX;
	bt_id_get(ids, &id_count);
	zassert_equal(id_count, 1,
		"Expect only one ID to be present after bt_enable()");

	/* More configurations to be tested:
	 *
	 * - identity is public/random
	 * - Running an initiator initiator
	 * - Execute test with multiple identities */

	ztest_test_suite(test_legacy_adv,
			 ztest_unit_test(test_legacy_adv_no_scan),
			 ztest_unit_test(test_legacy_adv_passive_scan),
			 ztest_unit_test(test_legacy_adv_active_scan)
			 );

	/* TODO: Add test suite for extended advertising types. */

	/* TODO: Add test suite for starting scanner/initiator
	 * when advertiser is running. */

	ztest_run_test_suite(test_legacy_adv);
}
