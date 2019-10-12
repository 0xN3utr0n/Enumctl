/*
* Simple program which lists all running processes
* using the Enumctl library.
*/


#include "Enumctl.h"

enum { NORMAL_MODE, DAEMON_MODE };


static void
help (void)
{
    printf("\n./Enumctl <opt>\
            \n\t -u Show all users' processes\
            \n\t -d Show only active daemons\n\n");

    exit(EXIT_SUCCESS);
}




static void 
print_list (ProcessInfo_t * list, const int flag)
{
    for (size_t i = 0; list[i].cgroup; i++)
    {
        if (flag == DAEMON_MODE)
        {
            printf(GREEN "\n\t● ");
            printf(RESET "%s", list[i].cgroup);
        }
            
        if (list[i].uid > 0)
                printf(RESET "\n\t%d", list[i].uid);
        else
                printf(RESET "\n\t?");

        printf("\t%d\t%s\n", list[i].pid, list[i].cmdline);
            
        if (strlen(list[i].cmdline) > 2) free(list[i].cmdline); //If the string is not '?'
        if (strlen(list[i].cgroup) > 2) free(list[i].cgroup);
    }

    free(list);
}




int
main (int argc, char ** argv)
{
    if (argc != 2)
            help();

    if (check_init() == false)
            exit(EXIT_FAILURE);

    printf(GREEN "\n\t  ### ENUMCTL ###\n");
    printf(RESET "\n\tUID\tPID\tCOMMAND\n");
    puts("\t-----------------------");
    
    ProcessInfo_t * proc_list = NULL;
    uint8_t status            = 0;

    if (!strncmp(argv[1], "-u", 2))
    {
        proc_list = get_process_list("user-*.slice"); //List all users' processes
        status    = NORMAL_MODE;
    }
    else if (!strncmp(argv[1], "-d", 2))
    {
        proc_list = get_process_list("*.service"); //List all running daemons
        status    = DAEMON_MODE;
    }    
    else
        help();

    print_list(proc_list, status);
}
