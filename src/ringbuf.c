#include "../include/ringbuf.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define TIMEOUT 1

uint8_t *write_inc(rbctx_t *context, uint8_t *writer_ptr) {
  writer_ptr += 1;
  if (writer_ptr >= context->end) {
    writer_ptr = context->begin;
  }
  return writer_ptr;
}

void read_inc(rbctx_t *context) {
  context->read += 1;
  if (context->read >= context->end) {
    context->read = context->begin;
  }
}

size_t write_space(rbctx_t *context) {
  if (context->write >= context->read) {
    return context->end - context->write + context->read - context->begin - 1;
  } else {
    return context->read - context->write - 1;
  }
}

size_t read_space(rbctx_t *context) {
  if (context->write >= context->read) {
    return context->write - context->read;
  } else {
    return context->end - context->read + context->write - context->begin;
  }
}

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

int ringbuffer_write(rbctx_t *context, void *message, size_t message_len) {
  pthread_mutex_lock(&context->mutex_write);

  while (write_space(context) < message_len + sizeof(size_t)) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += TIMEOUT;

    if (pthread_cond_timedwait(&context->signal_write, &context->mutex_write,
                               &ts) != 0) {
      pthread_mutex_unlock(&context->mutex_write);
      return RINGBUFFER_FULL;
    }
  }

  uint8_t *write = context->write;
  for (size_t i = 0; i < sizeof(message_len); i++) {
    *write = (uint8_t)((message_len >> (8 * i)) & 0xFF);
    write = write_inc(context, write);
  }

  for (size_t i = 0; i < message_len; i++) {
    *write = ((char *)message)[i];
    write = write_inc(context, write);
  }

  context->write = write;

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
    message_len |= (uint8_t)*context->read << (8 * i);
    read_inc(context);
  }

  if (message_len > *buffer_len) {
    pthread_mutex_unlock(&context->mutex_read);
    return OUTPUT_BUFFER_TOO_SMALL;
  }

  while (read_space(context) < message_len) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += TIMEOUT;

    if (pthread_cond_timedwait(&context->signal_read, &context->mutex_read,
                               &ts) != 0) {
      pthread_mutex_unlock(&context->mutex_read);
      return RINGBUFFER_EMPTY;
    }
  }

  *buffer_len = message_len;
  for (size_t i = 0; i < message_len; i++) {
    ((char *)buffer)[i] = *context->read;
    read_inc(context);
  }

  pthread_cond_signal(&context->signal_write);
  pthread_mutex_unlock(&context->mutex_read);
  return SUCCESS;
}

void ringbuffer_destroy(rbctx_t *context) {
  pthread_mutex_destroy(&context->mutex_write);
  pthread_mutex_destroy(&context->mutex_read);
  pthread_cond_destroy(&context->signal_read);
  pthread_cond_destroy(&context->signal_write);
}
