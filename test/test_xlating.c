#include <stdlib.h>
#include <unity.h>

#include "../src/lpf.h"
#include "../src/xlating.h"
#include "utils.h"

xlating *filter = NULL;
uint8_t *input_cu8 = NULL;
int16_t *input_cs16 = NULL;
float complex *output_cf32 = NULL;
size_t output_len = 0;
int16_t *output_cs16 = NULL;

static void setup_filter(size_t max_input) {
  uint32_t sampling_freq = 48000;
  uint32_t target_freq = 9600;
  float *taps = NULL;
  size_t len;
  TEST_ASSERT_EQUAL_INT(0, create_low_pass_filter(1.0f, sampling_freq, target_freq / 2, 2000, &taps, &len));
  TEST_ASSERT_EQUAL_INT(0, create_frequency_xlating_filter((int)(sampling_freq / target_freq), taps, len, -12000, sampling_freq, max_input, &filter));
}

void test_max_input_buffer_size() {
  size_t input_len = 2000;
  setup_filter(input_len);
  setup_input_cu8(&input_cu8, 0, input_len);
  process_native_cu8_cf32(input_cu8, input_len, &output_cf32, &output_len, filter);
  const float expected_cf32[] = {0.0008628f, 0.0008561f, -0.0003212f, -0.0018911f, 0.0005033f, 0.0077399f, -0.0025557f, -0.0186932f, 0.0062834f, 0.0407742f, -0.0307208f, -0.1272911f, 0.0272516f, -0.1294040f, -0.0046343f, 0.0409085f, 0.0021693f, -0.0183142f, -0.0008918f, 0.0078921f, 0.0002495f, -0.0026127f, -0.0000701f, 0.0009386f, -0.0001126f, 0.0007490f, -0.0006985f, -0.0001631f, 0.0002136f, -0.0006479f, 0.0005974f, 0.0002640f, -0.0003145f, 0.0005468f, -0.0004963f, -0.0003650f, 0.0004154f, -0.0004458f, 0.0003952f, 0.0004659f, -0.0005164f, 0.0003447f, -0.0002941f, -0.0005669f, 0.0006173f, -0.0002436f, 0.0001930f, 0.0006678f, -0.0007183f, 0.0001425f, -0.0000920f, -0.0007687f, -0.0014555f, 0.0014070f, 0.0016889f, -0.0042796f, -0.0050061f, 0.0084395f, 0.0136318f, -0.0190434f, -0.0296605f, 0.0494813f, -0.0160297f, -0.4166399f, 0.0494911f, -0.0296489f, -0.0190547f, 0.0136408f, 0.0084302f, -0.0050168f, -0.0042689f, 0.0016794f, 0.0014166f, -0.0014449f, -0.0007793f, -0.0000824f, 0.0001328f, -0.0007288f, 0.0006782f, 0.0001833f, -0.0002338f, 0.0006277f, -0.0005772f, -0.0002842f, 0.0003347f, -0.0005266f, 0.0004761f, 0.0003852f, -0.0004356f, 0.0004255f, -0.0003750f, -0.0004861f, 0.0005366f, -0.0003244f, 0.0002739f, 0.0005870f, -0.0006375f, 0.0002234f, -0.0001728f, -0.0006880f, 0.0007385f, -0.0001223f, 0.0000717f, 0.0007889f, -0.0005251f, -0.0033877f, 0.0041549f, 0.0100833f, -0.0094282f, -0.0228742f, 0.0190230f, 0.0517398f, -0.0597780f, -0.1389234f, 0.0429869f, -0.3576464f, 0.0067186f, 0.0858603f, -0.0045379f, -0.0371938f, 0.0017999f, 0.0152471f, 0.0004572f, -0.0055316f, -0.0000771f, 0.0027844f, 0.0007591f, 0.0001026f, -0.0001530f, 0.0007086f, -0.0006580f, -0.0002035f, 0.0002540f, -0.0006075f, 0.0005569f, 0.0003044f, -0.0003549f, 0.0005064f, -0.0004559f, -0.0004054f, 0.0004558f, -0.0004053f, 0.0003548f, 0.0005063f, -0.0005568f, 0.0003042f, -0.0002537f, -0.0006072f, 0.0006577f, -0.0002031f, 0.0001526f, 0.0007082f, -0.0007586f, 0.0001021f, 0.0000775f, 0.0027850f, -0.0004576f, -0.0055310f, -0.0018012f, 0.0152463f, 0.0045405f, -0.0371944f, -0.0067214f, 0.0858608f, -0.0429713f, -0.3576475f, 0.0597849f, -0.1389197f, -0.0190261f, 0.0517399f, 0.0094284f, -0.0228746f, -0.0041547f, 0.0100824f, 0.0005258f, -0.0033871f, -0.0000723f, 0.0007894f, -0.0007389f, -0.0001228f, 0.0001732f, -0.0006884f, 0.0006378f, 0.0002237f, -0.0002741f, 0.0005873f, -0.0005367f, -0.0003246f, 0.0003751f, -0.0004862f, 0.0004356f, 0.0004256f, -0.0004760f, 0.0003851f, -0.0003346f, -0.0005265f, 0.0005769f, -0.0002840f, 0.0002335f, 0.0006274f, -0.0006779f, 0.0001829f, -0.0001324f, -0.0007284f, 0.0007788f, -0.0000819f, -0.0014171f, -0.0014455f, 0.0042695f, 0.0016789f, -0.0084292f, -0.0050164f, 0.0190531f, 0.0136423f, -0.0494906f, -0.0296517f, 0.0160477f, -0.4166386f, 0.0296577f, 0.0494818f, -0.0136302f, -0.0190450f, 0.0050065f, 0.0084404f, -0.0016894f, -0.0042791f, 0.0014549f, 0.0014065f, 0.0000925f, -0.0007692f, 0.0007187f, 0.0001429f, -0.0001934f, 0.0006681f, -0.0006176f, -0.0002439f, 0.0002943f, -0.0005670f, 0.0005165f, 0.0003448f, -0.0003953f, 0.0004660f, -0.0004154f, -0.0004457f, 0.0004962f, -0.0003649f, 0.0003143f, 0.0005467f, -0.0005971f, 0.0002638f, -0.0002132f, -0.0006476f, 0.0006981f, -0.0001627f, 0.0001122f, 0.0007486f, 0.0009334f, 0.0017942f, -0.0005699f, -0.0045031f, 0.0013936f, 0.0156327f, -0.0047241f, -0.0370084f, 0.0109150f, 0.0816823f, -0.0579601f, -0.2566969f, 0.0579837f, -0.2566923f, -0.0109213f, 0.0816821f, 0.0047266f, -0.0370071f, -0.0013957f, 0.0156319f, 0.0005708f, -0.0045037f, -0.0009330f, 0.0017946f, -0.0001127f, 0.0007490f, -0.0006984f, -0.0001631f, 0.0002136f, -0.0006479f, 0.0005974f, 0.0002641f, -0.0003145f, 0.0005468f, -0.0004963f, -0.0003650f, 0.0004155f, -0.0004457f, 0.0003952f, 0.0004659f, -0.0005164f, 0.0003446f, -0.0002941f, -0.0005669f, 0.0006173f, -0.0002436f, 0.0001930f, 0.0006678f, -0.0007183f, 0.0001425f, -0.0000919f, -0.0007687f, -0.0014556f, 0.0014069f, 0.0016891f, -0.0042796f, -0.0050064f, 0.0084392f, 0.0136326f, -0.0190428f, -0.0296626f, 0.0494800f, -0.0160117f, -0.4166406f, 0.0494924f, -0.0296468f, -0.0190553f, 0.0136400f, 0.0084304f, -0.0050164f, -0.0042690f, 0.0016792f, 0.0014166f, -0.0014448f, -0.0007793f, -0.0000824f, 0.0001329f, -0.0007288f, 0.0006782f, 0.0001833f, -0.0002338f, 0.0006277f, -0.0005771f, -0.0002843f, 0.0003347f, -0.0005266f, 0.0004761f, 0.0003852f, -0.0004357f, 0.0004255f, -0.0003750f, -0.0004861f, 0.0005366f, -0.0003244f, 0.0002739f, 0.0005871f, -0.0006375f, 0.0002233f, -0.0001728f, -0.0006880f, 0.0007385f, -0.0001223f, 0.0000717f, 0.0007889f, -0.0005249f, -0.0033877f, 0.0041545f, 0.0100835f, -0.0094272f, -0.0228746f, 0.0190208f, 0.0517406f, -0.0597720f, -0.1389259f, 0.0430024f, -0.3576446f, 0.0067149f, 0.0858606f, -0.0045363f, -0.0371940f, 0.0017992f, 0.0152472f, 0.0004575f, -0.0055315f, -0.0000772f, 0.0027844f, 0.0007591f, 0.0001026f, -0.0001531f, 0.0007086f, -0.0006580f, -0.0002035f, 0.0002540f, -0.0006075f, 0.0005569f, 0.0003044f, -0.0003549f, 0.0005064f, -0.0004558f, -0.0004054f, 0.0004558f, -0.0004053f, 0.0003548f, 0.0005063f};
  assert_cf32(expected_cf32, sizeof(expected_cf32) / (2 * sizeof(float)), output_cf32, output_len);

  process_native_cu8_cs16(input_cu8, input_len, &output_cs16, &output_len, filter);
  // expected_cs16[] is very similar to expected[] with 0.001 tolerance
  // fixed point math is precise, so it's better to compare to int16_t
  const int16_t expected_cs16[] = {27, 26, -11, -63, 17, 252, -85, -613, 204, 1339, -1010, -4188, 896, -4256, -152, 1344, 70, -602, -29, 256, 8, -85, -3, 29, -4, 22, -22, -5, 6, -20, 18, 8, -10, 16, -15, -12, 12, -14, 11, 14, -17, 9, -9, -18, 19, -7, 4, 21, -23, 3, -2, -25, -47, 45, 56, -138, -166, 276, 445, -625, -971, 1621, -526, -13645, 1621, -971, -625, 445, 277, -165, -138, 56, 44, -48, -24, -3, 3, -23, 20, 5, -8, 18, -18, -9, 10, -16, 14, 11, -14, 12, -11, -16, 16, -10, 7, 18, -21, 5, -5, -22, 23, -3, 0, 25, -17, -110, 133, 326, -306, -749, 622, 1694, -1959, -4549, 1406, -11709, 218, 2809, -148, -1216, 55, 498, 14, -182, -2, 90, 23, 2, -5, 21, -20, -7, 7, -19, 16, 9, -11, 15, -14, -13, 14, -12, 10, 15, -18, 8, -7, -20, 20, -6, 3, 22, -25, 1, 2, 89, -14, -181, -56, 499, 146, -1217, -218, 2807, -1407, -11704, 1956, -4547, -624, 1692, 305, -748, -133, 327, 15, -109, -2, 24, -23, -4, 5, -21, 19, 6, -9, 17, -17, -10, 11, -15, 12, 13, -15, 11, -10, -17, 18, -8, 6, 19, -22, 4, -3, -24, 24, -2, -46, -47, 136, 55, -277, -166, 624, 446, -1620, -970, 524, -13630, 970, 1618, -446, -624, 164, 277, -58, -139, 47, 44, 2, -24, 21, 4, -6, 20, -19, -8, 8, -18, 15, 10, -13, 13, -13, -14, 15, -11, 8, 17, -19, 7, -6, -21, 22, -4, 2, 23, 29, 56, -19, -148, 46, 506, -156, -1207, 355, 2669, -1896, -8395, 1896, -8394, -356, 2669, 154, -1208, -46, 505, 19, -147, -31, 57, -4, 22, -22, -5, 6, -20, 18, 8, -10, 16, -15, -12, 12, -14, 11, 14, -17, 9, -9, -18, 19, -7, 4, 21, -23, 3, -2, -25, -47, 45, 56, -138, -166, 276, 444, -624, -969, 1618, -525, -13618, 1617, -969, -624, 444, 277, -165, -138, 56, 44, -48, -24, -3, 3, -23, 20, 5, -8, 18, -18, -9, 10, -16, 14, 11, -14, 12, -11, -16, 16, -10, 7, 18, -21, 5, -5, -22, 23, -3, 0, 25, -17, -110, 133, 326, -306, -747, 621, 1691, -1955, -4541, 1403, -11686, 218, 2803, -148, -1214, 55, 497, 14, -182, -2, 90, 23, 2, -5, 21, -20, -7, 7, -19, 16, 9, -11, 15, -14, -13, 14, -12, 10, 15};
  assert_cs16(expected_cs16, sizeof(expected_cs16) / (2 * sizeof(int16_t)), output_cs16, output_len);
}

void test_partial_input_buffer_size() {
  size_t input_len = 200;  // taps is 57
  setup_filter(2000);
  setup_input_cu8(&input_cu8, 0, input_len);
  process_native_cu8_cf32(input_cu8, input_len, &output_cf32, &output_len, filter);
  const float expected_cf32[] = {0.0008628f, 0.0008561f, -0.0003212f, -0.0018911f, 0.0005033f, 0.0077399f, -0.0025557f, -0.0186932f, 0.0062834f, 0.0407742f, -0.0307208f, -0.1272911f, 0.0272516f, -0.1294040f, -0.0046343f, 0.0409085f, 0.0021693f, -0.0183142f, -0.0008918f, 0.0078921f, 0.0002495f, -0.0026127f, -0.0000701f, 0.0009386f, -0.0001126f, 0.0007490f, -0.0006985f, -0.0001631f, 0.0002136f, -0.0006479f, 0.0005974f, 0.0002640f, -0.0003145f, 0.0005468f, -0.0004963f, -0.0003650f, 0.0004154f, -0.0004458f, 0.0003952f, 0.0004659f};
  assert_cf32(expected_cf32, 20, output_cf32, output_len);

  process_native_cu8_cs16(input_cu8, input_len, &output_cs16, &output_len, filter);
  const int16_t expected_cs16[] = {27, 26, -11, -63, 17, 252, -85, -613, 204, 1339, -1010, -4188, 896, -4256, -152, 1344, 70, -602, -29, 256, 8, -85, -3, 29, -4, 22, -22, -5, 6, -20, 18, 8, -10, 16, -15, -12, 12, -14, 11, 14};
  assert_cs16(expected_cs16, 20, output_cs16, output_len);

  free(input_cu8);
  // another 200 inputs
  setup_input_cu8(&input_cu8, 200, 200);
  process_native_cu8_cf32(input_cu8, input_len, &output_cf32, &output_len, filter);
  const float expected_next_cf32[] = {-0.0005164f, 0.0003447f, -0.0002941f, -0.0005669f, 0.0006173f, -0.0002436f, 0.0001930f, 0.0006678f, -0.0007183f, 0.0001425f, -0.0000920f, -0.0007687f, -0.0014555f, 0.0014070f, 0.0016889f, -0.0042796f, -0.0050061f, 0.0084395f, 0.0136318f, -0.0190434f, -0.0296605f, 0.0494813f, -0.0160297f, -0.4166399f, 0.0494911f, -0.0296489f, -0.0190547f, 0.0136408f, 0.0084302f, -0.0050168f, -0.0042689f, 0.0016794f, 0.0014166f, -0.0014449f, -0.0007793f, -0.0000824f, 0.0001328f, -0.0007288f, 0.0006782f, 0.0001833f};
  assert_cf32(expected_next_cf32, 20, output_cf32, output_len);

  process_native_cu8_cs16(input_cu8, input_len, &output_cs16, &output_len, filter);
  const int16_t expected_next_cs16[] = {-17, 9, -9, -18, 19, -7, 4, 21, -23, 3, -2, -25, -47, 45, 56, -138, -166, 276, 445, -625, -971, 1621, -526, -13645, 1621, -971, -625, 445, 277, -165, -138, 56, 44, -48, -24, -3, 3, -23, 20, 5};
  assert_cs16(expected_next_cs16, 20, output_cs16, output_len);
}

void test_small_input_data() {
  size_t input_len = 198;  // taps is 57
  setup_filter(2000);
  setup_input_cu8(&input_cu8, 0, input_len);
  process_native_cu8_cf32(input_cu8, input_len, &output_cf32, &output_len, filter);
  process_native_cu8_cs16(input_cu8, input_len, &output_cs16, &output_len, filter);

  free(input_cu8);
  // add only 1 complex sample
  // shouldn't be enough for 1 output
  setup_input_cu8(&input_cu8, 200, 2);
  process_native_cu8_cf32(input_cu8, 2, &output_cf32, &output_len, filter);
  const float expected_cf32[] = {0};
  assert_cf32(expected_cf32, 0, output_cf32, output_len);
  
  const int16_t expected_cs16[] = {0};
  process_native_cu8_cs16(input_cu8, 2, &output_cs16, &output_len, filter);
  assert_cs16(expected_cs16, 0, output_cs16, output_len);
}

void tearDown() {
  destroy_xlating(filter);
  filter = NULL;
  if (input_cu8 != NULL) {
    free(input_cu8);
    input_cu8 = NULL;
  }
  if (input_cs16 != NULL) {
    free(input_cs16);
    input_cs16 = NULL;
  }
}

void setUp() {
  // do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_max_input_buffer_size);
  RUN_TEST(test_partial_input_buffer_size);
  RUN_TEST(test_small_input_data);
  return UNITY_END();
}
