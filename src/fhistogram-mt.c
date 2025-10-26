// Setting _DEFAULT_SOURCE is necessary to activate visibility of
// certain header file contents on GNU/Linux systems.
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

#include "job_queue.h"

#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// err.h contains various nonstandard BSD extensions, but they are
// very handy.
#include <err.h>

#include "histogram.h"

int global_histogram[8] = { 0 };

struct job_queue queue;


struct thread_args {
    int id;
    int naptime;
};

int fhistogram_mt(char const* path) {
    FILE* f = fopen(path, "r");

    int local_histogram[8] = { 0 };

    if (f == NULL) {
        fflush(stdout);
        warn("failed to open %s", path);
        return -1;
    }

    int i = 0;
    
    char c;
    while (fread(&c, sizeof(c), 1, f) == 1) {
        i++;
        update_histogram(local_histogram, c);
        if ((i % 100000) == 0) {
            pthread_mutex_lock(&mutex);
            merge_histogram(local_histogram, global_histogram);
            pthread_mutex_unlock(&mutex);

            pthread_mutex_lock(&mutex);
            print_histogram(global_histogram);
            pthread_mutex_unlock(&mutex);
        }
    }
    fclose(f);
    
    pthread_mutex_lock(&mutex);
    merge_histogram(local_histogram, global_histogram);
    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&mutex);
    print_histogram(global_histogram);
    pthread_mutex_unlock(&mutex);
    
    return 0;

}

void* thread(void* arg) { //void* p
    struct thread_args* args = (struct thread_args*) arg;
    char* path;
    while (job_queue_pop(&queue, (void*)&path) == 0) {
        fhistogram_mt(path);
        free(path);
    }
    return NULL;
}

int main(int argc, char * const *argv) {
  if (argc < 2) {
    err(1, "usage: paths...");
    exit(1);
  }

  int num_threads = 1;
  char * const *paths = &argv[1];

  if (argc > 3 && strcmp(argv[1], "-n") == 0) {
    // Since atoi() simply returns zero on syntax errors, we cannot
    // distinguish between the user entering a zero, or some
    // non-numeric garbage.  In fact, we cannot even tell whether the
    // given option is suffixed by garbage, i.e. '123foo' returns
    // '123'.  A more robust solution would use strtol(), but its
    // interface is more complicated, so here we are.
    num_threads = atoi(argv[2]);

    if (num_threads < 1) {
      err(1, "invalid thread count: %s", argv[2]);
    }

    paths = &argv[3];
  } else {
    paths = &argv[1];
  }

  //Init job queue and threads
  int capacity = 64;
  job_queue_init(&queue, capacity);
  pthread_t ptids[num_threads];
  struct thread_args args[num_threads];

  pthread_mutex_init(&mutex, NULL);

  for (int i = 0; i < num_threads; i++) {
      args[i].id = i;
      args[i].naptime = 1;
      pthread_create(&ptids[i], NULL, thread, &args[i]);

  }

  // FTS_LOGICAL = follow symbolic links
  // FTS_NOCHDIR = do not change the working directory of the process
  //
  // (These are not particularly important distinctions for our simple
  // uses.)

  //File processing
  int fts_options = FTS_LOGICAL | FTS_NOCHDIR;

  FTS *ftsp;
  if ((ftsp = fts_open(paths, fts_options, NULL)) == NULL) {
    err(1, "fts_open() failed");
    return -1;
  }

  FTSENT *p;
  while ((p = fts_read(ftsp)) != NULL) {
    switch (p->fts_info) {
    case FTS_D:
      break;
    case FTS_F:
      //Processing the file p->fts_path.
        printf("Queued file: %s\n", p->fts_path);
        job_queue_push(&queue, strdup(p->fts_path));
      break;
    default:
      break;
    }
  }
  fts_close(ftsp);

  //Destorys mutex and job queue
  pthread_mutex_destroy(&mutex);
  job_queue_destroy(&queue);

  //Join the threads
  for (int j = 0; j < num_threads; j++) {
      if (pthread_join(ptids[j], NULL) != 0) {
          err(1, "pthread_join failed");
      }
  }
  pthread_mutex_lock(&mutex);
  print_histogram(global_histogram);
  pthread_mutex_unlock(&mutex);

  move_lines(9);

  return 0;
}
