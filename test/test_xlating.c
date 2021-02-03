#include <stdlib.h>
#include <check.h>
#include "utils.h"
#include "../src/xlating.h"
#include "../src/lpf.h"

xlating *filter = NULL;
uint8_t *input = NULL;

void assert_complex(const float expected[], size_t expected_size, float complex *actual, size_t actual_size) {
	ck_assert_int_eq(expected_size, actual_size);
	for (size_t i = 0, j = 0; i < expected_size * 2; i += 2, j++) {
		ck_assert_int_eq((int32_t ) expected[i] * 10000, (int32_t ) crealf(actual[j]) * 10000);
		ck_assert_int_eq((int32_t ) expected[i + 1] * 10000, (int32_t ) cimagf(actual[j]) * 10000);
	}
}

void setup_filter(size_t input_offset, size_t input_len, size_t max_input) {
	uint32_t sampling_freq = 48000;
	uint32_t target_freq = 9600;
	float *taps = NULL;
	size_t len;
	int code = create_low_pass_filter(1.0, sampling_freq, target_freq / 2, 2000, &taps, &len);
	ck_assert_int_eq(code, 0);
	code = create_frequency_xlating_filter((int) (sampling_freq / target_freq), taps, len, -12000, sampling_freq, max_input, &filter);
	ck_assert_int_eq(code, 0);
	setup_input_data(&input, input_offset, input_len);
}

START_TEST (test_max_input_buffer_size) {
	size_t input_len = 2000;
	setup_filter(0, input_len, input_len);
	float complex *output;
	size_t output_len = 0;
	process(input, input_len, &output, &output_len, filter);
	const float expected[] = { 0.000863, 0.000856, -0.000321, -0.001891, 0.000503, 0.007740, -0.002556, -0.018693, 0.006283, 0.040774, -0.030721, -0.127291, 0.027252, -0.129404, -0.004634, 0.040908, 0.002169, -0.018314, -0.000892, 0.007892, 0.000249, -0.002613, -0.000070, 0.000939, -0.000113, 0.000749, -0.000698, -0.000163, 0.000214, -0.000648, 0.000597, 0.000264, -0.000315, 0.000547, -0.000496, -0.000365, 0.000415, -0.000446, 0.000395, 0.000466, -0.000516, 0.000345, -0.000294, -0.000567, 0.000617, -0.000244, 0.000193, 0.000668, -0.000718, 0.000143, -0.000092, -0.000769,
			-0.001456, 0.001407, 0.001689, -0.004280, -0.005006, 0.008439, 0.013632, -0.019043, -0.029660, 0.049481, -0.016030, -0.416640, 0.049491, -0.029649, -0.019055, 0.013641, 0.008430, -0.005017, -0.004269, 0.001679, 0.001417, -0.001445, -0.000779, -0.000082, 0.000133, -0.000729, 0.000678, 0.000183, -0.000234, 0.000628, -0.000577, -0.000284, 0.000335, -0.000527, 0.000476, 0.000385, -0.000436, 0.000426, -0.000375, -0.000486, 0.000537, -0.000324, 0.000274, 0.000587, -0.000638, 0.000223, -0.000173, -0.000688, 0.000738, -0.000122, 0.000072, 0.000789, -0.000525, -0.003388,
			0.004155, 0.010083, -0.009428, -0.022874, 0.019023, 0.051740, -0.059778, -0.138923, 0.042987, -0.357646, 0.006719, 0.085860, -0.004538, -0.037194, 0.001800, 0.015247, 0.000457, -0.005532, -0.000077, 0.002784, 0.000759, 0.000103, -0.000153, 0.000709, -0.000658, -0.000203, 0.000254, -0.000607, 0.000557, 0.000304, -0.000355, 0.000506, -0.000456, -0.000405, 0.000456, -0.000405, 0.000355, 0.000506, -0.000557, 0.000304, -0.000254, -0.000607, 0.000658, -0.000203, 0.000153, 0.000708, -0.000759, 0.000102, 0.000078, 0.002785, -0.000458, -0.005531, -0.001801, 0.015246,
			0.004541, -0.037194, -0.006721, 0.085861, -0.042971, -0.357647, 0.059785, -0.138920, -0.019026, 0.051740, 0.009428, -0.022875, -0.004155, 0.010082, 0.000526, -0.003387, -0.000072, 0.000789, -0.000739, -0.000123, 0.000173, -0.000688, 0.000638, 0.000224, -0.000274, 0.000587, -0.000537, -0.000325, 0.000375, -0.000486, 0.000436, 0.000426, -0.000476, 0.000385, -0.000335, -0.000526, 0.000577, -0.000284, 0.000233, 0.000627, -0.000678, 0.000183, -0.000132, -0.000728, 0.000779, -0.000082, -0.001417, -0.001445, 0.004269, 0.001679, -0.008429, -0.005016, 0.019053, 0.013642,
			-0.049491, -0.029652, 0.016048, -0.416638, 0.029658, 0.049482, -0.013630, -0.019045, 0.005006, 0.008440, -0.001689, -0.004279, 0.001455, 0.001407, 0.000092, -0.000769, 0.000719, 0.000143, -0.000193, 0.000668, -0.000618, -0.000244, 0.000294, -0.000567, 0.000517, 0.000345, -0.000395, 0.000466, -0.000415, -0.000446, 0.000496, -0.000365, 0.000314, 0.000547, -0.000597, 0.000264, -0.000213, -0.000648, 0.000698, -0.000163, 0.000112, 0.000749, 0.000933, 0.001794, -0.000570, -0.004503, 0.001394, 0.015633, -0.004724, -0.037008, 0.010915, 0.081682, -0.057960, -0.256697,
			0.057984, -0.256692, -0.010921, 0.081682, 0.004727, -0.037007, -0.001396, 0.015632, 0.000571, -0.004504, -0.000933, 0.001795, -0.000113, 0.000749, -0.000698, -0.000163, 0.000214, -0.000648, 0.000597, 0.000264, -0.000315, 0.000547, -0.000496, -0.000365, 0.000415, -0.000446, 0.000395, 0.000466, -0.000516, 0.000345, -0.000294, -0.000567, 0.000617, -0.000244, 0.000193, 0.000668, -0.000718, 0.000142, -0.000092, -0.000769, -0.001456, 0.001407, 0.001689, -0.004280, -0.005006, 0.008439, 0.013633, -0.019043, -0.029663, 0.049480, -0.016012, -0.416641, 0.049492, -0.029647,
			-0.019055, 0.013640, 0.008430, -0.005016, -0.004269, 0.001679, 0.001417, -0.001445, -0.000779, -0.000082, 0.000133, -0.000729, 0.000678, 0.000183, -0.000234, 0.000628, -0.000577, -0.000284, 0.000335, -0.000527, 0.000476, 0.000385, -0.000436, 0.000426, -0.000375, -0.000486, 0.000537, -0.000324, 0.000274, 0.000587, -0.000638, 0.000223, -0.000173, -0.000688, 0.000738, -0.000122, 0.000072, 0.000789, -0.000525, -0.003388, 0.004154, 0.010083, -0.009427, -0.022875, 0.019021, 0.051741, -0.059772, -0.138926, 0.043002, -0.357644, 0.006715, 0.085861, -0.004536, -0.037194,
			0.001799, 0.015247, 0.000457, -0.005532, -0.000077, 0.002784, 0.000759, 0.000103, -0.000153, 0.000709, -0.000658, -0.000204, 0.000254, -0.000607, 0.000557, 0.000304, -0.000355, 0.000506, -0.000456, -0.000405, 0.000456, -0.000405, 0.000355, 0.000506 };
	assert_complex(expected, sizeof(expected) / (2 * sizeof(float)), output, output_len);
}
END_TEST

START_TEST (test_parital_input_buffer_size) {
	size_t input_len = 200; // taps is 57
	setup_filter(0, input_len, 2000);
	float complex *output;
	size_t output_len = 0;
	process(input, input_len, &output, &output_len, filter);
	const float expected[] = { 0.000863, 0.000856, -0.000321, -0.001891, 0.000503, 0.007740, -0.002556, -0.018693, 0.006283, 0.040774, -0.030721, -0.127291, 0.027252, -0.129404, -0.004634, 0.040908, 0.002169, -0.018314, -0.000892, 0.007892, 0.000249, -0.002613, -0.000070, 0.000939, -0.000113, 0.000749, -0.000698, -0.000163, 0.000214, -0.000648, 0.000597, 0.000264, -0.000315, 0.000547, -0.000496, -0.000365, 0.000415, -0.000446, 0.000395, 0.000466 };
	assert_complex(expected, 20, output, output_len);
	free(input);
	// another 200 inputs
	setup_input_data(&input, 200, 200);
	process(input, input_len, &output, &output_len, filter);
	const float expectedNextBatch[] = { -0.000516, 0.000345, -0.000294, -0.000567, 0.000617, -0.000244, 0.000193, 0.000668, -0.000718, 0.000143, -0.000092, -0.000769, -0.001456, 0.001407, 0.001689, -0.004280, -0.005006, 0.008439, 0.013632, -0.019043, -0.029660, 0.049481, -0.016030, -0.416640, 0.049491, -0.029649, -0.019055, 0.013641, 0.008430, -0.005017, -0.004269, 0.001679, 0.001417, -0.001445, -0.000779, -0.000082, 0.000133, -0.000729, 0.000678, 0.000183 };
	assert_complex(expectedNextBatch, 20, output, output_len);
}
END_TEST

START_TEST (test_small_input_data) {
	size_t input_len = 198; // taps is 57
	setup_filter(0, input_len, 2000);
	float complex *output;
	size_t output_len = 0;
	process(input, input_len, &output, &output_len, filter);
	free(input);
	// add only 1 complex sample
	// shouldn't be enough for 1 output
	setup_input_data(&input, 200, 2);
	process(input, 2, &output, &output_len, filter);
	const float expectedNextBatch[] = { 0 };
	assert_complex(expectedNextBatch, 0, output, output_len);
}
END_TEST

void teardown() {
	destroy_xlating(filter);
	filter = NULL;
	if (input != NULL) {
		free(input);
		input = NULL;
	}
}

void setup() {
//do nothing
}

Suite* common_suite(void) {
	Suite *s;
	TCase *tc_core;

	s = suite_create("xlating");

	/* Core test case */
	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_max_input_buffer_size);
	tcase_add_test(tc_core, test_parital_input_buffer_size);
	tcase_add_test(tc_core, test_small_input_data);

	tcase_add_checked_fixture(tc_core, setup, teardown);
	suite_add_tcase(s, tc_core);

	return s;
}

int main(void) {
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = common_suite();
	sr = srunner_create(s);

	srunner_set_fork_status(sr, CK_NOFORK);
	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

