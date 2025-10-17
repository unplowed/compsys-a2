#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "job_queue.h"
#include "pthread.h"

int job_queue_init(struct job_queue *job_queue, int capacity) {
  job_queue->capacity = capacity;
  job_queue->size = 0;
  job_queue->data = calloc(capacity, sizeof(void *));

  job_queue->mutex = malloc(sizeof(pthread_mutex_t));

  if (pthread_mutex_init(job_queue->mutex, NULL) < 0)
    return -1;

  job_queue->cond_job_popped = malloc(sizeof(pthread_cond_t));
  if (pthread_cond_init(job_queue->cond_job_popped, NULL) < 0)
    return -1;

  job_queue->cond_job_pushed = malloc(sizeof(pthread_cond_t));
  if (pthread_cond_init(job_queue->cond_job_pushed, NULL) < 0)
    return -1;

  job_queue->destroyed = 0;

  return 0;
}

int job_queue_destroy(struct job_queue *job_queue) {
  job_queue->destroyed = 1;

  while (job_queue->size != 0) {
  }

  if (pthread_cond_broadcast(job_queue->cond_job_pushed) < 0)
    return -1;

  return 0;
}

int job_queue_push(struct job_queue *job_queue, void *data) {
  if (pthread_mutex_lock(job_queue->mutex) < 0)
    return -1;

  if (job_queue->destroyed) {
    pthread_mutex_unlock(job_queue->mutex);
    return -1;
  }

  if (job_queue->size > job_queue->capacity) {
    // block until an element is popped
    if (pthread_cond_wait(job_queue->cond_job_popped, job_queue->mutex) < 0)
      return -1;
  }

  job_queue->data[job_queue->size++] = data;

  if (pthread_mutex_unlock(job_queue->mutex) < 0)
    return -1;
  if (pthread_cond_signal(job_queue->cond_job_pushed) < 0)
    return -1;

  return 0;
}

int job_queue_pop(struct job_queue *job_queue, void **data) {
  if (pthread_mutex_lock(job_queue->mutex) < 0)
    return -1;

  if (job_queue->size == 0) {
    if (job_queue->destroyed) {
      pthread_mutex_unlock(job_queue->mutex);
      return -1;
    }

    // block until an element is pushed
    if (pthread_cond_wait(job_queue->cond_job_pushed, job_queue->mutex) < 0)
      return -1;

    if (job_queue->destroyed) {
      pthread_mutex_unlock(job_queue->mutex);
      return -1;
    }
  }

  *data = job_queue->data[--job_queue->size];

  if (pthread_mutex_unlock(job_queue->mutex) < 0)
    return -1;
  if (pthread_cond_signal(job_queue->cond_job_popped) < 0)
    return -1;

  return 0;
}
