#ifndef ENUMCTL_H
#define ENUMCTL_H

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-login.h>
#include <stdbool.h>
#include <errno.h>


#define GREEN           "\033[32m"
#define RESET           "\033[0m"
#define RED             "\033[31m"

/*
 * Systemd's internal structure
 */
typedef struct
{
    const char *machine;
    const char *id;
    const char *description;
    const char *load_state;
    const char *active_state;
    const char *sub_state;
    const char *following;
    const char *unit_path;
    uint32_t    job_id;
    const char *job_type;
    const char *job_path;
} UnitInfo_t;


typedef struct
{
    char *cgroup;
    char *cmdline;
    pid_t pid;
    uid_t uid;
} ProcessInfo_t;


extern bool check_init(void);
extern ProcessInfo_t *get_process_list(const char *);


#endif