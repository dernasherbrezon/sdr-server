#include <unity.h>
#include "../src/queue.h"

queue *queue_obj = NULL;

void assert_buffers(const uint8_t *expected, size_t expected_len, uint8_t *actual, size_t actual_len) {
  TEST_ASSERT_EQUAL_INT(expected_len, actual_len);
  for (int i = 0; i < expected_len; i++) {
    TEST_ASSERT_EQUAL_INT(expected[i], actual[i]);
  }
}

void assert_buffer(const uint8_t *expected, int expected_len) {
  uint8_t *result = NULL;
  size_t len = 0;
  take_buffer_for_processing(&result, &len, queue_obj);
  // TODO doesn't work. segmentation fault if result is null
  TEST_ASSERT(result != NULL);
  assert_buffers(expected, expected_len, result, len);
  complete_buffer_processing(queue_obj);
}

void test_terminated_only_after_fully_processed() {
  int code = create_queue(262144, 10, &queue_obj);
  TEST_ASSERT_EQUAL_INT(code, 0);

  const uint8_t buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  queue_put(buffer, sizeof(buffer), queue_obj);

  interrupt_waiting_the_data(queue_obj);

  assert_buffer(buffer, 10);
}

void test_put_take() {
  int code = create_queue(262144, 10, &queue_obj);
  TEST_ASSERT_EQUAL_INT(code, 0);

  const uint8_t buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  queue_put(buffer, sizeof(buffer), queue_obj);

  const uint8_t buffer2[2] = {1, 2};
  queue_put(buffer2, sizeof(buffer2), queue_obj);

  assert_buffer(buffer, 10);
  assert_buffer(buffer2, 2);
}

void test_overflow() {
  int code = create_queue(262144, 1, &queue_obj);
  TEST_ASSERT_EQUAL_INT(code, 0);

  const uint8_t buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  queue_put(buffer, sizeof(buffer), queue_obj);
  const uint8_t buffer2[9] = {11, 12, 13, 14, 15, 16, 17, 18, 19};
  queue_put(buffer2, sizeof(buffer2), queue_obj);

  assert_buffer(buffer2, 9);
}

void tearDown() {
  destroy_queue(queue_obj);
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_put_take);
  RUN_TEST(test_overflow);
  RUN_TEST(test_terminated_only_after_fully_processed);
  return UNITY_END();
}

