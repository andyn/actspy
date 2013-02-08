/*
 * actspy.c
 *
 *
 * Watches the utmp file and TTYSs, and reports which users have been
 * active during the given poll period.
 *
 * Usage:
 * actspy [-bhio] [-t TIME] [USER...]
 *   -b               Ring the bell when a TTY is active.
 *   -h               Show this help
 *   -i               Show when a TTY receives input data.
 *   -o               Show when a TTY sends output data.
 *   -t TIME          Poll interval in seconds. Default and minimum 1.
 *   USER             One or more users to limit polling to.
 *
 * Known issues:
 * - UNIX file timestamps use one second resolution, so this
 *   program does not support subsecond poll intervals.
 *
 * Authors:
 * Antti Nilakari <antti.nilakari@gmail.com> 2012-05-14 20:34
 */

#define _POSIX_SOURCE
#define _XOPEN_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>
#include <utmpx.h>

static volatile int signal_caught_sigint = 0;

void signal_sigint(int signal_number) {
    (void) signal_number;
    signal_caught_sigint = 1;
}

/* Like strstr, but instead of a haystack string use a list of strings
 * to compare against.
 * Returns a pointer to the matching string, or NULL. */
char *strstrlist(char ** haystack, const char * needle) {
    for (; *haystack; haystack++) {
        /* Compare current haystack entry against the needle */
        if (!strcmp(*haystack, needle)) {
            break;
        }
    }
       
    return *haystack;
}

struct options {
    time_t poll_interval;
    int watch_input;
    int watch_output;
    int bell;
    char ** watched_users;
};

/* Parse command line options and return them. See struct options.
 * Returns 0 if succeeded.
 * Returns 1 if a parameter requested a graceful exit (e.g. -h for help).
 * Returns -1 if parsing command line parameters failed */ 
int options_parse(int argc, char ** argv, struct options * options) {
    /* Default one second */
    options->poll_interval = 1;
    /* Default poll flags (in, out, bell) */
    options->watch_input = 0;
    options->watch_output = 0;
    options->bell = 0;
    /* Default list of users to watch */
    options->watched_users = NULL;
    
    extern char * optarg;
    extern int optind, opterr, optopt;
    opterr = 0;
    int option_char;
    while ((option_char = getopt(argc, argv, "bhiot:")) != -1) {
        switch (option_char) {
        case 'b':
            options->bell = 1;
            break;
        case 'h':
            printf(
                "Usage: %s [-bhio] [-t TIME] [USER]...\n"
                "  -b       Ring the bell when a TTY is active.\n"
                "  -h       Show this help.\n"
                "  -i       Show when a TTY receives input data.\n"
                "  -o       Show when a TTY sends output data.\n"
                "  -t TIME  Poll interval in seconds. Default and minimum 1.\n"
                "  USER     Limit polling to one or more users.\n",
                argv[0]);
            return 1;
            break;
        case 'i':    
            options->watch_input = 1;
            break;
        case 'o':
            options->watch_output = 1;
            break;
        case 't':
            if (sscanf(optarg, "%ld", &options->poll_interval) == 0) {
                fprintf(stderr, "Invalid timestamp option -t %s.\n", optarg);
                return -1;
            }
            break;
        case '?':
            switch (optopt) {
            case 't':
                fprintf(stderr, "Option -%c requires an argument.\n",
                	optopt);
            	break;
            default:
                fprintf(stderr, "Unknown command line option -%c.\n",
                    optopt);
                break;
            }
            return -1;
            break;
        }
    }
    /* The rest of cmdline params are users we want to watch.
     * Always non-NULL. Points to NULL if no users were specified. */
    options->watched_users = argv + optind;

    return 0;
}

int main(int argc, char ** argv) {
    signal(SIGINT, signal_sigint);

    struct options options;
    if (options_parse(argc, argv, &options)) {
        return EXIT_FAILURE;
    }

    /* We want to ignore our own tty since it'll always be active */
    const char * my_tty_path = ttyname(STDOUT_FILENO);
    
    time_t last_poll = 0;
    char * last_poll_string = NULL;
    do {
        last_poll = time(NULL);
        last_poll_string = ctime(&last_poll);
        /* Wipe off the newline in the timestamp string */
        last_poll_string[strlen(last_poll_string) - 1] = '\0';
        sleep(options.poll_interval); /* or SIGINT */
        
        setutxent(); /* Begin UTMP processing */
        struct utmpx * utmp_entry;
        while ((utmp_entry = getutxent()) != NULL) {
            /* Skip uninteresting processes */
            if (utmp_entry->ut_type != USER_PROCESS) {
                continue;
            }
            /* Generate the full TTY path, needs /dev/ prefix */
            char tty_path[6 + UT_LINESIZE] = "/dev/";
            strncat(tty_path, utmp_entry->ut_line, sizeof(tty_path));
            /* Ignore our TTY */
            if (strcmp(my_tty_path, tty_path) == 0) {
                continue;
            }
            
            /* If access time on the TTY has changed, print out the user name.
             * On a TTY device, st_atime is reset whenever a user types
             * anything in the terminal. Likewise, st_mtime is reset when
             * a terminal puts out anything. */
            struct stat tty_stat;
            stat(tty_path, &tty_stat);
            if ((options.watch_input && (tty_stat.st_atime >= last_poll)) ||
                (options.watch_output && (tty_stat.st_mtime >= last_poll))) {
                /* If a list of users is specified, limit the search
                   to those users */
                if (*options.watched_users &&
                    !strstrlist(options.watched_users,
                    utmp_entry->ut_user)) {
                    continue; /* Next _utmp_ round */
                }
                /* Ring the bell if requested */
                if (options.bell) {
                    printf("\a");
                }
                /* Print the line */
                printf("%s; %s; %s\n",
                    last_poll_string,
                    utmp_entry->ut_line,
                    utmp_entry->ut_user);
            }
        }
        endutxent(); /* End UTMP processing */
  
    } while (!signal_caught_sigint);
}

