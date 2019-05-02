/*
 * Enumctl.c
 *
 * Copyright 2019 0xN3utr0n <0xN3utr0n at pm.me>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */


#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-login.h>
#include <stdbool.h>
#include <errno.h>

#define BUF_SIZE        30
#define DAEMON_MODE     1
#define NORMAL_MODE     0
#define GREEN           "\033[32m"
#define RESET           "\033[0m"
#define RED             "\033[31m"
//#define DEBUG



/*
 * As far as I know, it's an internal structure.
 * So we must define it ourself.
 */
typedef struct UnitInfo
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
} UnitInfo;


uint8_t read_message(sd_bus_message *, UnitInfo *, char *);
static bool get_user_id(pid_t, uid_t *, char *);
sd_bus_message * request_unit_list(sd_bus *, char *);
sd_bus_message * request_unit_info(sd_bus *, UnitInfo *);
bool parse_unit_info(sd_bus_message *, UnitInfo *, int);
UnitInfo * parse_unit_list(sd_bus_message *);
static void init(const int, const char *);
static bool check_init(void);
static void help(void);




/*
 * sud_bus_message_read() function wrapper.
 */

uint8_t
read_message (sd_bus_message *message, UnitInfo *u, char *pattern)
{
    int err =   sd_bus_message_read(
                            message,
                            pattern,
                            &u->id,
                            &u->description,
                            &u->load_state,
                            &u->active_state,
                            &u->sub_state,
                            &u->following,
                            &u->unit_path,
                            &u->job_id,
                            &u->job_type,
                            &u->job_path);

    if (err < 0)
         perror("Unable to read bus data");

    return err;
}




static bool
get_user_id (pid_t pid, uid_t * uid, char *group)
{
    //With procfs hardening enabled, it won't work out.
    if (sd_pid_get_owner_uid(pid, uid) < 0)
    { //If it's a user-owned process, this may be.
        if (strstr(group, "user"))
            sscanf(group, "user%*c%d", uid);
        else
            return false;
    }

    return true;
}




/*
 * Request to systemd, through sd-bus daemon,
 * all active units which match @pattern.
 * @return the message object.
 */

sd_bus_message *
request_unit_list (sd_bus *bus, char *pattern)
{
    sd_bus_error error  = SD_BUS_ERROR_NULL;
    sd_bus_message *m   = NULL;
    int err             = 0;

    err =  sd_bus_call_method(bus,
                        "org.freedesktop.systemd1",
                        "/org/freedesktop/systemd1",
                        "org.freedesktop.systemd1.Manager",
                        "ListUnitsByPatterns",              /* Method name */
                        &error,
                        &m,                                 /* Return message */
                        "asas",                             /* Arg. Signature */
                        1, "active",                        /* Arguments */
                        1, pattern);                        /* Arguments */

    if (err < 0)
    {
        fprintf(stderr, "\n Failed to request process \
                 list: %s\n", error.message);
        goto finish;
    }

    /* In order to process a container-message (arrays, structs...),
    it's required to use this function. */
    err = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "(ssssssouso)");
    if (err < 0)
    {
        fprintf(stderr, "\n Failed to parse unit \
                list (container1): %s\n", strerror(-err));
        goto finish;
    }

finish:
    sd_bus_error_free(&error);
    return err < 0 ? NULL : m;
}




/*
 * Request to systemd, through D-bus daemon,
 * more information about a certain unit.
 * @return the message object.
 */

sd_bus_message *
request_unit_info (sd_bus *bus, UnitInfo *u)
{
    sd_bus_error error  = SD_BUS_ERROR_NULL;
    sd_bus_message *m   = NULL;
    int err             = 0;

    err =  sd_bus_call_method(bus,
                    "org.freedesktop.systemd1",
                    "/org/freedesktop/systemd1",
                    "org.freedesktop.systemd1.Manager",
                    "GetUnitProcesses",                     /* Method name */
                    &error,
                    &m,                                     /* Return message */
                    "s", u->id);                            /* Signature & Argument*/

    if (err < 0)
    {
        fprintf(stderr, "\nFailed to request Unit %s: %s\n",
                u->id, error.message);
        goto finish;
    }

    err = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "(sus)");
    if (err < 0)
    {
        fprintf(stderr, "\nFailed to parse unit %s (container1): %s\n",
                u->id, strerror(-err));
         goto finish;
    }

finish:
    sd_bus_error_free(&error);
    return err < 0 ? NULL : m;
}




/*
 * The final step in order to output all the processes data.
 * Some Units answers, such as Apparmor's, may be empty because
 * we are unauthorized to obtain more information about them.
 */

bool
parse_unit_info (sd_bus_message *m, UnitInfo *u, int flag)
{
    uid_t uid       = 0;
    pid_t pid       = 0;
    char *cmdline   = NULL;
    char *group     = NULL;
    int err         = 0;
    bool stat       = true;

    // Each unit's message is an array of processes.
    while ((err = sd_bus_message_read(m, "(sus)", NULL, &pid, &cmdline)))
    {
        if (!pid) //This should never fail
            continue;

        if(!cmdline)
            cmdline = "?";

        group = (char *) u->id; //unit name == cgroup name

        if (flag == DAEMON_MODE)
        {
            printf(GREEN "\n\t● ");
            printf(RESET "%s", u->id);
        }

        if (get_user_id(pid, &uid, group))
            printf("\n\t%d", uid);
        else
            printf("\n\t?");

        printf("\t%d\t%s\n", pid, cmdline);
    }

    if (err < 0)
    {
        fprintf(stderr, "\nFailed to read unit %s: %s\n",
                u->id, strerror(-err));
        stat = false;
    }

    sd_bus_message_exit_container(m);
    sd_bus_message_exit_container(m);

    return stat;
}




/*
 * Just parse the answer and create
 * an array with information about
 * each unit.
 * @return a pointer to the list.
 */

UnitInfo *
parse_unit_list (sd_bus_message *m)
{
    UnitInfo u          = {0};
    UnitInfo * list     = NULL;
    size_t size         = BUF_SIZE;

    if ((list = calloc(sizeof(UnitInfo), size)) == NULL)
    {
        perror("Unable to allocate memory");
        goto finish;
    }

    //parse the answer, field by field
    for (size_t i = 0; read_message(m, &u, "(ssssssouso)"); i++)
    {
        if (i == size)
        { //increase array
            size += BUF_SIZE;
            if ((list = realloc(list, size * sizeof(UnitInfo))) == NULL)
            {
                perror("Unable to allocate memory");
                goto finish;
            }
        }

        memcpy(&list[i], &u, sizeof(UnitInfo));

#ifdef DEBUG
        printf("\t %p -- %s\n", &list[i], list[i].id);
#endif
    }

finish:
    sd_bus_message_exit_container(m);
    return list;
}




/*
 * Big picture of the program's inner workings.
 */

static void
init (const int flag, const char *pattern)
{
    sd_bus *bus             = NULL;
    sd_bus_message *m       = NULL;
    uint8_t  r              = 0;
    UnitInfo *proc_list     = NULL;

    if ((sd_bus_open_system(&bus)) < 0) //Connect to system bus
    {
        perror("**[CRITICAL]** System bus");
        exit(EXIT_FAILURE);
     }

#ifdef DEBUG
    printf(RED "\n\t## Enumctl DEBUG MODE ##\n\n");
    printf(RESET "     -------------------------------\n");
#else
    printf(GREEN "\n\t  ### ENUMCTL ###\n");
    printf(RESET "\n\tUID\tPID\tCOMMAND\n");
    puts("\t-----------------------");
#endif

    if ((m = request_unit_list(bus, (char*)pattern)) == NULL)
    {
        r = EXIT_FAILURE;
        goto finish;
    }

    if ((proc_list = parse_unit_list(m)) == NULL)
    {
        r = EXIT_FAILURE;
        goto finish;
    }

    for (uint32_t i = 0; proc_list[i].id; i++)
    {
        if ((m = request_unit_info(bus, &proc_list[i])) == NULL)
            continue;

#ifdef DEBUG
        printf(RED "\n\t %p -- %s \n", &proc_list[i], proc_list[i].id);
        printf(RESET);
#endif

        parse_unit_info(m, &proc_list[i], flag);
    }

    r = EXIT_SUCCESS;
    free(proc_list);

finish:
    sd_bus_message_unref(m); //Clean up
    sd_bus_flush_close_unref(bus);
    exit(r);
}




/*
 * Use a few methods to check which kind
 * of init process is running.
 */

static bool
check_init (void)
{
    if (sd_booted())
        return true;

    char buff[BUF_SIZE + 1]  = {0};
    char *initd              = NULL;
    FILE *fp                 = NULL;

    if ((fp = fopen("/proc/1/status", "r")) == NULL) //init process -> pid 1
    {
        perror("File /proc/1/status not available");
        return false;
    }

    if (fgets(buff, BUF_SIZE, fp) == NULL)
    {
        perror("Failed to read status file");
        goto exit;
    }

    strtok(buff, ":");
    initd = strtok(NULL, ":");

    if (!strstr(initd, "systemd"))
    {
        fprintf(stderr, "\n**[CRITCAL]** Init process: %s\n", initd);
        errno = ENOPKG;
    }

exit:
    fclose(fp);
    return (errno)? false : true;
}




static void
help (void)
{
    printf("\n./Enumctl <opt>\
            \n\t -u Show all users processes\
            \n\t -d Show only active daemons\n\n");

    exit(EXIT_SUCCESS);
}




int
main (int argc, char ** argv)
{
    if (argc != 2)
        help();

    if (check_init() == false)
        exit(EXIT_FAILURE);


    if (!strncmp(argv[1], "-u", 2))
        init(NORMAL_MODE, "user-*.slice");

    else if (!strncmp(argv[1], "-d", 2))
        init(DAEMON_MODE, "*.service");

    else
        help();
}
