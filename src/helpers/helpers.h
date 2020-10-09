#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <linux/rtnetlink.h>
#include <linux/veth.h>
#include <net/if.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#define N_PARAMS 2
#define BUFF_SIZE 300
#define MAX_BUF_SIZE 100
#ifndef ISOLATE_NETNS_H
#define ISOLATE_NETNS_H


#define DEBUG 0

/* commands */
#define RUN 1
#define RUN_COMMAND "run"
#define CHILD 2
#define CHILD_COMMAND "child"
#define COMMAND_NOT_FOUND -1

#define printErr(msg)   do { \
                            fprintf(stdout, "[ERROR] - %s failed (errno: %d): %s\n", msg, errno, strerror(errno)); \
                            exit(EXIT_FAILURE); \
                           } while (0)

#define debug_print(msg) \
	do { if (DEBUG) fprintf(stdout, "[DEBUG] - %s", msg); } while (0)

/* clone args structure */
struct clone_args {
   unsigned int n_args;
   char **argv;
   int pipe_fd[2];  /* Pipe used to synchronize parent and child */
};

/* print the usage of the tool */
void printUsage();

/* dispatch che input command */
int parser(char *command);

/* print the container pid and command name */
void printRunningInfos(struct clone_args *args);

#define MAX_PAYLOAD 1024

struct nl_req {
    struct nlmsghdr n;
    struct ifinfomsg i;
    char buf[MAX_PAYLOAD];
};

#define NLMSG_TAIL(nmsg) \
	((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

int create_socket(int domain, int type, int protocol);
void if_up(char *ifname, char *ip, char *netmask);
void create_veth(int sock_fd, char *ifname, char *peername);
void move_if_to_pid_netns(int sock_fd, char *ifname, int netns);
int get_netns_fd(int pid);

#endif //ISOLATE_NETNS_H
