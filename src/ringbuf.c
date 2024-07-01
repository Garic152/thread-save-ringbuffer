#include "../include/ringbuf.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define TIMEOUT 1

void ringbuffer_init(rbctx_t *context, void *buffer_location,
                     size_t buffer_size) {
  context->begin = (uint8_t *)buffer_location;
  context->end = context->begin + buffer_size;
  context->read = context->begin;
  context->write = context->begin;

  pthread_mutex_init(&context->mutex_read, NULL);
  pthread_mutex_init(&context->mutex_write, NULL);
  pthread_cond_init(&context->signal_read, NULL);
  pthread_cond_init(&context->signal_write, NULL);
}

int ringbuffer_write(rbctx_t *context, void *message, size_t message_len) {
  pthread_mutex_lock(&context->mutex_write);

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += TIMEOUT;
  int rt = 0;

  size_t free_space =
      (context->write > context->read)
          ? (context->end - context->write + context->read - context->begin - 1)
          : (context->read - context->write - 1);

  while (free_space < message_len + 1) {
    rt = pthread_cond_timedwait(&context->signal_write, &context->mutex_write,
                                &ts);
    if (rt != 0) {
      pthread_mutex_unlock(&context->mutex_write);
      return RBUF_TIMEOUT;
    }
    free_space = (context->write > context->read)
                     ? (context->end - context->write + context->read -
                        context->begin - 1)
                     : (context->read - context->write - 1);
  }

  context->write[0] = (uint8_t)message_len;
  context->write += 1;
  if (context->write >= context->end) {
    context->write = context->begin;
  }

  for (size_t i = 0; i < message_len; i++) {
    context->write[0] = ((char *)message)[i];
    context->write += 1;

    if (context->write >= context->end) {
      context->write = context->begin;
    }
  }

  pthread_mutex_unlock(&context->mutex_write);
  pthread_cond_signal(&context->signal_read);
  return SUCCESS;
}

int ringbuffer_read(rbctx_t *context, void *buffer, size_t *buffer_len) {
  pthread_mutex_lock(&context->mutex_read);

  if (context->read == context->write) {
    pthread_mutex_unlock(&context->mutex_read);
    return RINGBUFFER_EMPTY;
  }

  uint8_t message_len = context->read[0];
  if (message_len > *buffer_len) {
    pthread_mutex_unlock(&context->mutex_read);
    return OUTPUT_BUFFER_TOO_SMALL;
  }

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += TIMEOUT;
  int rt = 0;

  size_t read_available =
      (context->read > context->write)
          ? (context->end - context->read + context->write - context->begin - 1)
          : (context->write - context->read - 1);

  while (read_available < message_len) {
    rt = pthread_cond_timedwait(&context->signal_read, &context->mutex_read,
                                &ts);
    if (rt != 0) {
      pthread_mutex_unlock(&context->mutex_read);
      return RBUF_TIMEOUT;
    }
    read_available = (context->read > context->write)
                         ? (context->end - context->read + context->write -
                            context->begin - 1)
                         : (context->write - context->read - 1);
  }

  context->read += 1;
  if (context->read >= context->end) {
    context->read = context->begin;
  }

  *buffer_len = message_len;
  for (size_t i = 0; i < message_len; i++) {
    ((uint8_t *)buffer)[i] = context->read[0];
    context->read += 1;

    if (context->read >= context->end) {
      context->read = context->begin;
    }
  }

  pthread_mutex_unlock(&context->mutex_read);
  pthread_cond_signal(&context->signal_write);
  return SUCCESS;
}

void ringbuffer_destroy(rbctx_t *context) {
  pthread_mutex_destroy(&context->mutex_read);
  pthread_mutex_destroy(&context->mutex_write);
  pthread_cond_destroy(&context->signal_read);
  pthread_cond_destroy(&context->signal_write);
}
