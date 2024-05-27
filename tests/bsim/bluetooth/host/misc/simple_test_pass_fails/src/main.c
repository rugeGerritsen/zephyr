/*
 * Copyright (c) 2022 Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bstests.h"
#include "bs_types.h"
#include "bs_tracing.h"
#include "time_machine.h"

extern enum bst_result_t bst_result;

#define WAIT_TIME (600 * 1e6) /*seconds*/

#define PASS(...) \
	do { \
		bst_result = Passed; \
		bs_trace_info_time(1, __VA_ARGS__); \
	} while (0)

#define FAIL(...) \
	do { \
		bst_result = Failed; \
		bs_trace_error_time_line(__VA_ARGS__); \
	} while (0)

static void test_tick(bs_time_t HW_device_time)
{
	if (bst_result != Passed) {
		FAIL("test failed (not passed after %i seconds)\n", WAIT_TIME);
	}
}

static void test_init(void)
{
	bst_ticker_set_next_tick_absolute(WAIT_TIME);
	bst_result = In_progress;
}

static void test_simple(void)
{
	PASS("Test is passing");
}

static const struct bst_test_instance test_def[] = {
	{
		.test_id = "test_id",
		.test_post_init_f = test_init,
		.test_tick_f = test_tick,
		.test_main_f = test_simple
	},
	BSTEST_END_MARKER
};

struct bst_test_list *test_main(struct bst_test_list *tests)
{
	return bst_add_tests(tests, test_def);
}

bst_test_install_t test_installers[] = {
	test_main,
	NULL
};

int main(void)
{
	bst_main();
	return 0;
}
