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

static void check_adv_random_addr(struct bt_le_adv_param *adv_param,
								  bool active_scanner_or_initiator_enabled)
{
	bool expect_success = true;

	bt_addr_t random_addr;
	bool random_addr_valid;

	bool random_addr_shall_contain_identity =
		(adv_param->options & BT_LE_ADV_OPT_USE_IDENTITY);

	if (active_scanner_or_initiator_enabled) {
		/* Only accept when the advertiser and scanner/initiator are using
		 * the same address.
		 *
		 * TODO: Add other conditions
		 *  */

		if (random_addr_shall_contain_identity)
			expect_success = false;
	}

	if (expect_success) {
		zassert_equal(bt_le_adv_start(adv_param, NULL, 0, NULL, 0), 0,
				"Advertising start failed");
	} else {
		zassert_equal(bt_le_adv_start(adv_param, NULL, 0, NULL, 0), -EIO,
				"Advertising start succeeded");
		return;
	}

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

static void test_legacy_adv_scanner(bool active_scanner_or_initiator_enabled)
{
	struct bt_le_adv_param adv_param =
			BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_NONE,
					BT_GAP_ADV_FAST_INT_MIN_2,
					BT_GAP_ADV_FAST_INT_MIN_2,
					NULL);
	bool use_identity[] = {true};

	/* Undirected advertising types */
	uint32_t undirected_options[] =
	{
		BT_LE_ADV_OPT_NONE,

		/* TODO: When BT_LE_ADV_OPT_USE_IDENTITY is NOT set,
		 * we still advertise with identity address */
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

			check_adv_random_addr(&adv_param, active_scanner_or_initiator_enabled);
		}
	}

	/* Directed advertising types*/
	uint32_t directed_options[] =
	{
		BT_LE_ADV_OPT_CONNECTABLE,
		BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY,
	};

	adv_param.peer = &peer_addr;

	for (int i = 0; i < ARRAY_SIZE(directed_options); i++) {
		for (int j = 0; j < ARRAY_SIZE(use_identity); j++) {
			adv_param.options = directed_options[i];
			if (use_identity[j]) {
				adv_param.options |= BT_LE_ADV_OPT_USE_IDENTITY;
			}

			check_adv_random_addr(&adv_param, active_scanner_or_initiator_enabled);
		};
	}
}

/**
 * @brief Test integer arithmetic operations
 *
 * @details Test multiplication and division of two
 * integers
 */
void test_legacy_adv_no_scan(void)
{
	test_legacy_adv_scanner(false);
}

void test_legacy_adv_passive_scan(void)
{
	zassert_equal(bt_le_scan_start(BT_LE_SCAN_PASSIVE, NULL), 0,
			"Scan start failed");

	test_legacy_adv_scanner(false);

	zassert_equal(bt_le_scan_stop(), 0, "Scan stop failed");
}

void test_legacy_adv_active_scan(void)
{
	zassert_equal(bt_le_scan_start(BT_LE_SCAN_ACTIVE, NULL), 0,
			"Scan start failed");

	test_legacy_adv_scanner(true);

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

	/* Configurations to be tested:
	 *
	 * - identity is public/random
	 * - Passive scanner running at the same time -> no change
	 * - active scanner uses identity/NRPA
	 * - initiator
	 *
	 * TODO: Execute test with multiple identities */

	ztest_test_suite(test_legacy_adv,
			 ztest_unit_test(test_legacy_adv_no_scan),
			 ztest_unit_test(test_legacy_adv_passive_scan),
			 ztest_unit_test(test_legacy_adv_active_scan)
			 );

	ztest_run_test_suite(test_legacy_adv);
}
