#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include "network.h"
#include "../../helpers/helpers.h"

void prepare_netns(int cmd_pid)
{
    char *veth = "veth0";
    char *vpeer = "veth1";
    char *veth_addr = "10.1.1.1";
    char *vpeer_addr = "10.1.1.2";
    char *netmask = "255.255.255.0";  //10.1.1.0/24

    int sock_fd = create_socket(
            PF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);

    create_veth(sock_fd, veth, vpeer);

    if_up(veth, veth_addr, netmask);

    int mynetns = get_netns_fd(getpid());
    int child_netns = get_netns_fd(cmd_pid);

    move_if_to_pid_netns(sock_fd, vpeer, child_netns);

    if (setns(child_netns, CLONE_NEWNET))
        printErr("setns");

    if_up(vpeer, vpeer_addr, netmask);

    if (setns(mynetns, CLONE_NEWNET))
        printErr("restore previous net namespace");

    close(sock_fd);
}



void start_network(pid_t child_pid) {
    char buffer[MAX_BUF_SIZE];
    char *command = "/usr/local/bin/netsetgo -pid";
    snprintf(buffer, sizeof(buffer), "%s %d", command, child_pid);
    //fprintf(stdout,"buffer: %s", buffer);
    system(buffer);
}
