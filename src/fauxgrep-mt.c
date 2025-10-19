// Setting _DEFAULT_SOURCE is necessary to activate visibility of
// certain header file contents on GNU/Linux systems.
#define _DEFAULT_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fts.h>
#include <sys/stat.h>
#include <sys/types.h>

// err.h contains various nonstandard BSD extensions, but they are
// very handy.
#include <err.h>

#include <pthread.h>

#include "job_queue.h"

// Global mutex
static pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

// Struct
struct search_queue {
  struct job_queue *job_q;
  const char *needle;
};

int fauxgrep_file_mt(const char *needle, const char *path) {
  FILE *file = fopen(path, "r");

  if (file == NULL) {
    warn("failed to open %s", path);
    return -1;
  }

  char *line = NULL;
  size_t linelen = 0;
  int lineno = 1;

  while (getline(&line, &linelen, file) != -1) {
    if (strstr(line, needle) != NULL) {
      int rc = pthread_mutex_lock(&print_lock);
      // Locking and unlocking mutex
      assert(rc == 0);
      printf("%s:%d:%s", path, lineno, line);

      rc = pthread_mutex_unlock(&print_lock);
      assert(rc == 0);
    }
    lineno++;
  }
  free(line);
  fclose(file);
  return 0;
}

void *worker_threads(void *arg) {
  struct search_queue *sq_ptr = arg;

  while (1) {
    char *next_path;
    if (job_queue_pop(sq_ptr->job_q, (void **)&next_path) == 0) {
      fauxgrep_file_mt(sq_ptr->needle, next_path);
      free(next_path);
    } else {
      break;
    }
  }
  return NULL;
}

int main(int argc, char *const *argv) {
  if (argc < 2) {
    err(1, "usage: [-n INT] STRING paths...");
    exit(1);
  }

  // Init variables
  int num_threads = 1;
  char const *needle = argv[1];
  char *const *paths = &argv[2];

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

    needle = argv[3];
    paths = &argv[4];

  } else {
    needle = argv[1];
    paths = &argv[2];
  }

  // assert(0);
  //  Initialise the job queue and some worker threads here.

  struct job_queue job_q;
  job_queue_init(&job_q, 64);

  struct search_queue sq;
  struct search_queue *sqp = &sq;
  sqp->job_q = &job_q;
  sqp->needle = needle;

  // Init worker threads
  pthread_t *threads = calloc(num_threads, sizeof(pthread_t));
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&threads[i], NULL, worker_threads, sqp) != 0) {
      err(1, "pthread_create() failed");
    }
  }

  //------implementing programs here-----

  // JOB QUEUE

  // FTS_LOGICAL = follow symbolic links
  // FTS_NOCHDIR = do not change the working directory of the process
  //
  // (These are not particularly important distinctions for our simple
  // uses.)
  int fts_options = FTS_LOGICAL | FTS_NOCHDIR;

  // File traversal setup
  FTS *ftsp = fts_open(paths, fts_options, NULL);
  if (ftsp == NULL) {
    err(1, "fts_open() failed");
  }

  // Traversing the directory tree
  FTSENT *p;
  while ((p = fts_read(ftsp)) != NULL) {
    switch (p->fts_info) {
    case FTS_D:
      break;
    case FTS_F: {
      char *copy = strdup(p->fts_path);
      if (!copy)
        err(1, "strdup failed");
      if (job_queue_push(sqp->job_q, copy) != 0) {
        warn("job_queue_push failed");
        free(copy);
      }
      break;
    }
    default:
      break;
    }
  }

  fts_close(ftsp);

  // assert(0);
  //  Shut down the job queue and the worker threads here.
  //-----------shutting down job queue and worker threads.
  job_queue_destroy(sqp->job_q);

  for (int i = 0; i < num_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      err(1, "pthread_join() failed");
    }
  }

  free(threads);
  return 0;
}
