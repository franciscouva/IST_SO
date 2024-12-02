#define MAX_RESERVATION_SIZE 256
#define STATE_ACCESS_DELAY_US 500000  // 500ms
#define MAX_JOB_FILE_NAME_SIZE 256
#define MAX_SESSION_COUNT 8
#define MAX_PIPE_NAME 40

#include <pthread.h>

/* operation codes (for client-server requests) */
enum {
    EMS_OP_CODE_SETUP = 1,
    EMS_OP_CODE_QUIT = 2,
    EMS_OP_CODE_CREATE = 3,
    EMS_OP_CODE_RESERVE = 4,
    EMS_OP_CODE_SHOW = 5,
    EMS_OP_CODE_LIST = 6,
};