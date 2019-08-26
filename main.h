#ifndef __MAIN
#define __MAIN

#define BUFFER_LEN 65535
#define MAX_PORTS 65535

typedef struct {
    int timeout;
    char* command;
    int ports[MAX_PORTS];
} JOB, *PJOB;

#endif //__MAIN
