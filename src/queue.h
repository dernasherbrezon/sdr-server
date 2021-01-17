#ifndef QUEUE_H_
#define QUEUE_H_

typedef struct queue_t queue;

int create_queue(uint32_t buffer_size, int capacity, queue **queue);

void put(const uint8_t *buffer, const int len, queue *queue);
void take_buffer_for_processing(uint8_t **buffer, int *len, queue *queue);
void complete_buffer_processing(queue *queue);

void destroy_queue(queue *queue);

#endif /* QUEUE_H_ */
