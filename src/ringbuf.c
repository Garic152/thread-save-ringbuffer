#include "../include/ringbuf.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define TIMEOUT 1

void ringbuffer_init(rbctx_t *context, void *buffer_location,
                     size_t buffer_size) {
  context->begin = buffer_location;
  context->end = buffer_location + buffer_size;
  context->read = buffer_location;
  context->write = buffer_location;

  pthread_mutex_init(&context->mutex_read, NULL);
  pthread_mutex_init(&context->mutex_write, NULL);
  pthread_cond_init(&context->signal_read, NULL);
  pthread_cond_init(&context->signal_write, NULL);
}

size_t write_space(rbctx_t *context) {
  size_t space;

  if (context->write == context->read) {
    space = context->end - context->begin - 1;
  } else {
    space = (context->write > context->read)
                ? (context->end - context->write + context->read -
                   context->begin - 1)
                : (context->read - context->write - 1);
  }
  return space;
}

size_t read_space(rbctx_t *context) {
  size_t space;
  if (context->read == context->write) {
    space = 0;
  } else {
    space =
        (context->read > context->write)
            ? (context->end - context->read + context->write - context->begin)
            : (context->write - context->read);
  }
  return space;
}

int ringbuffer_write(rbctx_t *context, void *message, size_t message_len) {
  pthread_mutex_lock(&context->mutex_write);

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += TIMEOUT;
  int rt = 0;

  while (write_space(context) < message_len + sizeof(size_t)) {
    rt = pthread_cond_timedwait(&context->signal_write, &context->mutex_write,
                                &ts);
    if (rt != 0) {
      pthread_mutex_unlock(&context->mutex_write);
      return RINGBUFFER_FULL;
    }
  }

  for (size_t i = 0; i < sizeof(size_t); i++) {
    // *context->write = (uint8_t)((message_len >> (8 * i)) & 0xFF);
    *(uint8_t *)(context->write) = (uint8_t)((message_len >> (8 * i)) & 0xFF);
    context->write += 1;
    if (context->write >= context->end) {
      context->write = context->begin;
    }
  }

  for (size_t i = 0; i < message_len; i++) {
    *context->write = ((char *)message)[i];
    context->write += 1;

    if (context->write >= context->end) {
      context->write = context->begin;
    }
  }

  pthread_cond_signal(&context->signal_read);
  pthread_mutex_unlock(&context->mutex_write);
  return SUCCESS;
}

int ringbuffer_read(rbctx_t *context, void *buffer, size_t *buffer_len) {
  pthread_mutex_lock(&context->mutex_read);

  if (read_space(context) < sizeof(size_t)) {
    pthread_mutex_unlock(&context->mutex_read);
    return RINGBUFFER_EMPTY;
  }

  size_t message_len = 0;
  for (size_t i = 0; i < sizeof(size_t); i++) {
    // message_len |= (uint8_t)*context->read << (8 * i);
    message_len |= ((size_t)(*(uint8_t *)(context->read))) << (8 * i);
    context->read += 1;
    if (context->read >= context->end) {
      context->read = context->begin;
    }
  }

  if (message_len > *buffer_len) {
    pthread_mutex_unlock(&context->mutex_read);
    printf("HELP");
    return OUTPUT_BUFFER_TOO_SMALL;
  }

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += TIMEOUT;
  int rt = 0;

  while (read_space(context) < message_len) {
    rt = pthread_cond_timedwait(&context->signal_read, &context->mutex_read,
                                &ts);
    if (rt != 0) {
      pthread_mutex_unlock(&context->mutex_read);
      return RBUF_TIMEOUT;
    }
  }

  *buffer_len = message_len;
  for (size_t i = 0; i < message_len; i++) {
    ((char *)buffer)[i] = *context->read;
    context->read += 1;

    if (context->read >= context->end) {
      context->read = context->begin;
    }
  }

  pthread_cond_signal(&context->signal_write);
  pthread_mutex_unlock(&context->mutex_read);
  return SUCCESS;
}

void ringbuffer_destroy(rbctx_t *context) {
  pthread_mutex_destroy(&context->mutex_write);
  pthread_mutex_destroy(&context->mutex_write);
  pthread_cond_destroy(&context->signal_read);
  pthread_cond_destroy(&context->signal_write);
}
