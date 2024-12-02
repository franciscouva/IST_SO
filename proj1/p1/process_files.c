#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <fcntl.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"
#include "process_files.h"


void *process_event_file(void *arg) {
    while (1) {
        unsigned int event_id, delay;
        size_t num_rows, num_columns, num_coords;
        size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
        int break_while = 0;
        struct ThreadArgs *args = (struct ThreadArgs *) arg;

          if (pthread_mutex_lock(&mutex_main)) {
            fprintf(stderr, "Error locking mutex_main\n");
            exit(EXIT_FAILURE);
          }
          switch (get_next(args->fd)) {
            case CMD_CREATE:
              if (parse_create(args->fd, &event_id, &num_rows, &num_columns) != 0) {
                fprintf(stderr, "Invalid command. See HELP for usage\n");
                continue;
              }

              if (ems_create(event_id, num_rows, num_columns)) {
                fprintf(stderr, "Failed to create event\n");
              }

              break;

            case CMD_RESERVE:
              num_coords = parse_reserve(args->fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);

              if (num_coords == 0) {
                fprintf(stderr, "Invalid command. See HELP for usage\n");
                continue;
              }

              if (ems_reserve(event_id, num_coords, xs, ys)) {
                fprintf(stderr, "Failed to reserve seats\n");
              }

              break;

            case CMD_SHOW:
              if (parse_show(args->fd, &event_id) != 0) {
                fprintf(stderr, "Invalid command. See HELP for usage\n");
                continue;
              }

              if (ems_show(event_id, args->wd)) {
                fprintf(stderr, "Failed to show event\n");
              }

              break;

            case CMD_LIST_EVENTS:
              if (ems_list_events(args->wd)) {
                fprintf(stderr, "Failed to list events\n");
              }

              pthread_mutex_unlock(&mutex_main);
              break;

            case CMD_WAIT:
              int status = parse_wait(args->fd, &delay, NULL);
              if (status == -1) {
                fprintf(stderr, "Invalid command. See HELP for usage\n");
                continue;
              } else if (status == 0) {
                if (delay > 0) {
                  printf("Waiting...\n");
                  ems_wait(delay);
                }
              } else if (status == 1){
                if (args->thread_id >= max_threads) {
                  continue;
                }
                wait_flag = 1;
              }

              break;

            case CMD_INVALID:
              pthread_mutex_unlock(&mutex_main);
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              break;

            case CMD_HELP: {
              pthread_mutex_unlock(&mutex_main);
              const char *helpText =
                  "Available commands:\n"
                  "  CREATE <event_id> <num_rows> <num_columns>\n"
                  "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
                  "  SHOW <event_id>\n"
                  "  LIST\n"
                  "  WAIT <delay_ms> [thread_id]\n" 
                  "  BARRIER\n"                     
                  "  HELP\n";

              size_t helpTextLen = strlen(helpText);
              ssize_t bytes_written = write(args->wd, helpText, helpTextLen);
              if (bytes_written != (ssize_t)helpTextLen) {
                  fprintf(stderr, "Error writing to outfile\n");
                  close(args->fd);
                  close(args->wd);
                  exit(1);
              }
              break;
            }

            case CMD_BARRIER: 
              pthread_mutex_unlock(&mutex_main);
              barrier_flag = 1;
              break;

            case CMD_EMPTY:
              pthread_mutex_unlock(&mutex_main);
              break;

            case EOC:
              pthread_mutex_unlock(&mutex_main);
              break_while = 1;
              break;
          }
      if (barrier_flag) return (void *)1889;
      if (wait_flag) return (void *)1985;
      if (break_while) return 0;
    }
  return 0;
}
