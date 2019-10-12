#include "Enumctl.h"

#ifdef DEBUG
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__); 
#else
#define DEBUG_PRINT(...); //Do nothing
#endif

#define BUF_SIZE  30


static inline uint8_t read_message(sd_bus_message *, UnitInfo_t *, const char *);
static inline bool get_user_id(const pid_t, uid_t *, char *);
static inline bool resize_list(size_t *, void **, const size_t);
static sd_bus_message * request_unit_list(sd_bus *, const char *);
static sd_bus_message * request_unit_info(sd_bus *, UnitInfo_t *);
static void parse_unit_info(sd_bus_message *, UnitInfo_t *, ProcessInfo_t **);
static UnitInfo_t * parse_unit_list(sd_bus_message *);


/*
 * sud_bus_message_read() function wrapper.
 */

uint8_t
read_message (sd_bus_message *message, UnitInfo_t *u, const char *pattern)
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




bool
get_user_id (const pid_t pid, uid_t *uid, char *group)
{
    //With procfs hardening enabled, it won't work out.
    if (sd_pid_get_owner_uid(pid, uid) < 0)
    { //If it's a user-owned process rather than a root one, this may do.
        if (strstr(group, "user"))
                sscanf(group, "user%*c%d", uid);
        else
                return false;
    }

    return true;
}




/*
 * Request to systemd, through D-bus' daemon,
 * about all active units which match @pattern.
 * @return the message object.
 */

sd_bus_message *
request_unit_list (sd_bus *bus, const char *pattern)
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
                        1, (char*)pattern);                 /* Arguments */

    if (err < 0)
    {
        fprintf(stderr, "\n Failed to request unitess \
                 list: %s\n", error.message);
        goto finish;
    }

    /* In order to scan a container-message (arrays, structs...),
    it's required to use this function. */
    err = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "(ssssssouso)");
    if (err < 0)
    {
        fprintf(stderr, "\n Failed to parse unit \
                list (container1): %s\n", strerror(-err));
    }

finish:
    sd_bus_error_free(&error);
    return err < 0 ? NULL : m;
}




/*
 * Request to systemd, through D-bus' daemon,
 * more information about a certain unit.
 * @return the message object.
 */

sd_bus_message *
request_unit_info (sd_bus *bus, UnitInfo_t *u)
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
    }

finish:
    sd_bus_error_free(&error);
    return err < 0 ? NULL : m;
}




bool
resize_list(size_t *size, void **list, const size_t var_size) 
{
    void * temp_list = NULL;
    
    *size += BUF_SIZE;

    if ((temp_list = realloc(*list, *size * var_size)) == NULL)
    {
        perror("Unable to reallocate more memory");
        return false;
    }
    
    //Set the newly allocated memory to zero.
    memset(temp_list + ((*size - BUF_SIZE) * var_size), 0, BUF_SIZE * var_size); 
   
    *list = temp_list;
    return true;
}




/*
 * Scan a unit/cgroup and gather all processes which belong to it.
 * Some units answers, such as Apparmor's, might be empty because
 * we are unauthorized to obtain more information about them.
 */

void
parse_unit_info (sd_bus_message *m, UnitInfo_t *u, ProcessInfo_t **proc_list)
{
    ProcessInfo_t proc        = {0};
    static size_t index       = 0;
    static size_t size        = BUF_SIZE;
           

    // Each unit's message is an array of processes.
    for (; sd_bus_message_read(m, "(sus)", NULL, &proc.pid, &proc.cmdline); index++)
    {
        proc.cmdline = (proc.cmdline == NULL)? "?" : strdup(proc.cmdline);
        proc.uid     = (get_user_id(proc.pid, &proc.uid, (char *)u->id))? proc.uid : 0;
        proc.cgroup  = (u->id == NULL)? "?" : strdup(u->id);
        
        if (index == size)
        { 
            if (!resize_list(&size, (void**)proc_list, sizeof(ProcessInfo_t)))
                    goto finish;
        }
        
        memcpy(&(*proc_list)[index], &proc, sizeof(proc));
                
        DEBUG_PRINT(GREEN "\t%d) %p -- %s\n", index, &(*proc_list)[index], (*proc_list)[index].cmdline);
    }

    finish:
        sd_bus_message_exit_container(m);
        sd_bus_message_exit_container(m);
}




/*
 * Just parse the answer and create an array with information about
 * each unit.
 * @return a pointer to the list.
 */

UnitInfo_t *
parse_unit_list (sd_bus_message *m)
{
    UnitInfo_t u          = {0};
    UnitInfo_t * list     = NULL;
    size_t size           = BUF_SIZE;

    if ((list = calloc(size, sizeof(UnitInfo_t))) == NULL)
    {
        perror("Unable to allocate memory");
        goto finish;
    }

    //parse the answer, field by field
    for (size_t i = 0; read_message(m, &u, "(ssssssouso)"); i++)
    {
        if (i == size)
        {
            if (!resize_list(&size, (void**)&list, sizeof(UnitInfo_t)))
                    goto finish;
        }

        memcpy(&list[i], &u, sizeof(UnitInfo_t));

        DEBUG_PRINT(GREEN "\t %p -- %s\n", &list[i], list[i].id);
    }

finish:
    sd_bus_message_exit_container(m);
    return list;
}




/*
 * Scan all available units which match @pattern 
 * and insert their corresponding processes into an array.
 * @Return an array with all running processes.
 */

ProcessInfo_t *
get_process_list (const char *pattern)
{
    sd_bus *bus               = NULL;
    sd_bus_message *m         = NULL;
    UnitInfo_t *unit_list     = NULL;
    ProcessInfo_t *proc_list  = NULL;

    if ((sd_bus_open_system(&bus)) < 0) //Connect to system bus
            return NULL;

    if ((m = request_unit_list(bus, pattern)) == NULL)
            goto finish2;

    if ((unit_list = parse_unit_list(m)) == NULL)
            goto finish1;

    if ((proc_list = calloc(BUF_SIZE + 1, sizeof(ProcessInfo_t))) == NULL)
            goto finish;

    for (size_t i = 0; unit_list[i].id; i++)
    {
        if ((m = request_unit_info(bus, &unit_list[i])) == NULL)
                continue;

        DEBUG_PRINT(RED "\n\t %p -- %s \n", &unit_list[i], unit_list[i].id);

        parse_unit_info(m, &unit_list[i], &proc_list);
    }

finish:
    free(unit_list);
finish1:
    sd_bus_message_unref(m); //Clean up
finish2:
    sd_bus_flush_close_unref(bus);
    return proc_list;
}




/*
 * Use a few methods to check whether Systemd is running or not.
 */

bool
check_init (void)
{
    if (sd_booted())
            return true;

    char buff[BUF_SIZE + 1]  = {0};
    char *initd              = NULL;
    FILE *fp                 = NULL;

    if ((fp = fopen("/unit/1/status", "r")) == NULL) //init process -> pid 1
    {
        perror("File /unit/1/status not available");
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
        fprintf(stderr, "\nInit process: %s\n", initd);
        errno = ENOPKG;
    }

exit:
    fclose(fp);
    return (errno)? false : true;
}




