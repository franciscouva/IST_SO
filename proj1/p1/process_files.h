#ifndef PROCESS_EVENT_FILE_H
#define PROCESS_EVENT_FILE_H

#include <pthread.h>

struct ThreadArgs {
    int fd;
    int wd;
    int thread_id;
};

extern int max_threads;
extern pthread_mutex_t mutex_main;
extern pthread_mutex_t mutex_wrie;
extern pthread_mutex_t mutex_wait;
extern int barrier_flag;
extern int wait_flag;

void* process_event_file(void* arg);

#endif // PROCESS_EVENT_FILE_H