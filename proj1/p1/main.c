#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <fcntl.h>
#include <limits.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"
#include "process_files.h"


pthread_mutex_t mutex_main = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_write = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_wait = PTHREAD_MUTEX_INITIALIZER;
int max_threads;
int barrier_flag;
int wait_flag;


int main(int argc, char *argv[]) {
    unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
    int max_proc;
    DIR *dir;

    if (ems_init(state_access_delay_ms)) {
        fprintf(stderr, "Failed to initialize EMS\n");
        exit(EXIT_FAILURE);
    }

    if (argc == 4) {
        dir = opendir(argv[1]);
        if (!dir) {
            fprintf(stderr, "Unable to read directory\n");
            return 1;
        }
        max_proc = atoi(argv[2]);
        max_threads = atoi(argv[3]);
    }

    if (argc > 4) {
        dir = opendir(argv[1]);
        if (!dir) {
            fprintf(stderr, "Unable to read directory\n");
            return 1;
        }
        max_proc = atoi(argv[2]);
        max_threads = atoi(argv[3]);
        state_access_delay_ms = (unsigned int)strtoul(argv[4], NULL, STATE_ACCESS_DELAY_MS);
    }


    struct dirent *entry;
    int child_processes = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' || !strstr(entry->d_name, ".jobs")) {
            continue;
        }

        // Verifica se atingiu o número máximo de processos filhos em paralelo
        if (child_processes >= max_proc) {
            // Aguarda a conclusão de qualquer processo filho
            int status;
            pid_t finished_pid = wait(&status);

            // Imprime o estado de término correspondente
            fprintf(stdout, "Child process %d ended with exit code %d\n", finished_pid, WEXITSTATUS(status));

            child_processes--;
        }

        // Cria um processo filho
        pid_t pid = fork();

        if (pid < 0) {
            fprintf(stderr, "Error creating child process\n");
            return EXIT_FAILURE;
        } else if (pid == 0) {
            fprintf(stdout, "Child process opened %s\n", entry->d_name);
            
            char filePath[PATH_MAX];
            snprintf(filePath, sizeof(filePath), "%s/%s", argv[1], entry->d_name);

            int file_descriptor = open(filePath, O_RDONLY);
            if (file_descriptor == -1) {
                fprintf(stderr, "Error opening file %s\n", filePath);
                return 1;
            }

            char outputFileName[256];
            char outputFilePath[PATH_MAX];
            strcpy(outputFileName, entry->d_name);
            outputFileName[strlen(outputFileName) - 5] = '\0';
            snprintf(outputFilePath, sizeof(outputFilePath), "%s/%s.out", argv[1], outputFileName);
            int output_descriptor = open(outputFilePath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
            if (output_descriptor == -1) {
                fprintf(stderr, "Error creating outfile\n");
                close(file_descriptor);
                return 1;
            }

            pthread_t threads[max_threads];
            int num_threads = 0;
            struct ThreadArgs thread_args[max_threads];
            
            for (int i = 0; i < max_threads; i++) {
                thread_args[i].fd = file_descriptor;
                thread_args[i].wd = output_descriptor;
                thread_args[i].thread_id = i;
            }
            
            int count = max_threads;

            while (count == max_threads && count != 0) {
                count = 0;
                for (num_threads = 0; num_threads < max_threads; num_threads++) {
                    int status = pthread_create(&threads[num_threads], NULL, process_event_file, (void *)&thread_args[num_threads]);

                    if (status) {
                        fprintf(stderr, "Error creating thread in child process\n");
                        exit(EXIT_FAILURE);
                    }
                }

                int* thread_result;

                for (int i = 0; i < num_threads; ++i) {
                    thread_result = 0;
                    if (pthread_join(threads[i], (void**)&thread_result) != 0) {
                        fprintf(stderr, "Error joining thread\n");
                        exit(EXIT_FAILURE);
                    }
                    if (thread_result == (int *)1889) {
                        count++;
                    }
                    if (thread_result == (int *)1985) {
                        ems_wait(state_access_delay_ms);
                        count++;
                    }
                }
                barrier_flag = 0;
                wait_flag = 0;
            }

            close(file_descriptor);
            close(output_descriptor);
            exit(EXIT_SUCCESS);

        } else {
            if (child_processes >= max_proc) waitpid(pid, NULL, 0);
            child_processes++;
        }
    }

    // Aguarda a conclusão de todos os processos filhos restantes
    while (child_processes > 0) {
        int status;
        pid_t finished_pid = wait(&status);

        // Imprime o estado de término correspondente
        fprintf(stdout, "Child process %d ended with exit code %d\n", finished_pid, WEXITSTATUS(status));

        child_processes--;
    }

    ems_terminate();
    closedir(dir);
    return 0;
}