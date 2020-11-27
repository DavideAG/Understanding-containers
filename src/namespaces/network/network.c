#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>
#include <libiptc/libiptc.h>
#include <linux/netfilter/nf_nat.h>
#include <linux/netfilter/x_tables.h>
#include <arpa/inet.h>

#include "network.h"
#include "../../helpers/helpers.h"


void prepare_netns(int cmd_pid)
{
 /*
    char *veth = "veth0";
    char *vpeer = "veth1";
    char *veth_addr = "10.1.1.1";
    char *vpeer_addr = "10.1.1.2";
    char *netmask = "255.255.255.0";  //10.1.1.0/24
	int addrlen = sizeof(struct in_addr);
	struct in_addr addr;
	struct ifinfomsg *ifmsg;
*/
	// create socket
	int fd;
	if ((fd = _nl_socket_init()) == 0)
		exit(1);

	// create veth pair -> veth1 - vpeer1
	struct nlmsghdr *nlmsg = malloc(4096);
	memset(nlmsg, 0, 4096);
	nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	nlmsg->nlmsg_type  = RTM_NEWLINK;
	nlmsg->nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL|NLM_F_ACK;
	nlmsg->nlmsg_seq   = time(NULL);

	struct ifinfomsg *ifmsg;
	ifmsg = (struct ifinfomsg *) NLMSG_DATA(nlmsg);
	ifmsg->ifi_family = AF_UNSPEC;

	struct rtattr *nest1, *nest2, *nest3;

	NLMSG_STRING(nlmsg, IFLA_IFNAME, "veth1");

	nest1 = NLMSG_TAIL(nlmsg);
	NLMSG_ATTR(nlmsg, IFLA_LINKINFO);

	NLMSG_STRING(nlmsg, IFLA_INFO_KIND, "veth");

	nest2 = NLMSG_TAIL(nlmsg);
	NLMSG_ATTR(nlmsg, IFLA_INFO_DATA);

	nest3 = NLMSG_TAIL(nlmsg);
	NLMSG_ATTR(nlmsg, VETH_INFO_PEER);

	nlmsg->nlmsg_len += sizeof(struct ifinfomsg);

	NLMSG_STRING(nlmsg, IFLA_IFNAME, "vpeer1");

	nest3->rta_len = (unsigned char *)NLMSG_TAIL(nlmsg) - (unsigned char *)nest3;
	nest2->rta_len = (unsigned char *)NLMSG_TAIL(nlmsg) - (unsigned char *)nest2;
	nest1->rta_len = (unsigned char *)NLMSG_TAIL(nlmsg) - (unsigned char *)nest1;

	if (_nlmsg_send(fd, nlmsg) != 0)
		exit(1);

	if (_nlmsg_recieve(fd) != 0) {
		close(fd);
		exit(1);
	}

	int addrlen = sizeof(struct in_addr);
	free(nlmsg);
	nlmsg = malloc(4096);
	memset(nlmsg, 0, 4096);
	nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	nlmsg->nlmsg_flags = NLM_F_ACK|NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL;
	nlmsg->nlmsg_type  = RTM_NEWADDR;
	nlmsg->nlmsg_seq   = time(NULL);

	struct ifaddrmsg *ifa;
	ifa = (struct ifaddrmsg *) NLMSG_DATA(nlmsg);
	ifa->ifa_prefixlen = atoi("24");
	if (!(ifa->ifa_index = if_nametoindex("veth1"))) {
		printErr("failed to get veth1");
	}
	ifa->ifa_family = AF_INET;
	ifa->ifa_scope = 0;

	struct in_addr addr;
	struct in_addr bcast;

	if (inet_pton(AF_INET, "172.16.1.1", &addr) < 0)
		exit(1);
	if (inet_pton(AF_INET, "172.16.1.255", &bcast) < 0)
		exit(1);

	_nlmsg_put(nlmsg, IFA_LOCAL,     &addr,  addrlen);
	_nlmsg_put(nlmsg, IFA_ADDRESS,   &addr,  addrlen);
	_nlmsg_put(nlmsg, IFA_BROADCAST, &bcast, addrlen);

	if (_nlmsg_send(fd, nlmsg) != 0)
		exit(1);
	if (_nlmsg_recieve(fd) != 0) {
		close(fd);
		exit(1);
	}


/*
    int sock_fd = create_socket(
            PF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);

    create_veth(sock_fd, veth, vpeer);

    if_up(veth, veth_addr, netmask);
*/
    int mynetns = get_netns_fd(getpid());
    int child_netns = get_netns_fd(cmd_pid);

	// move vpeer1 in the child
	free(nlmsg);
	nlmsg = malloc(4096);
	memset(nlmsg, 0, 4096);
	nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	nlmsg->nlmsg_type  = RTM_NEWLINK;
	nlmsg->nlmsg_flags = NLM_F_REQUEST|NLM_F_ACK;
	nlmsg->nlmsg_seq   = time(NULL);

	ifmsg = (struct ifinfomsg *) NLMSG_DATA(nlmsg);
	ifmsg->ifi_family = AF_UNSPEC;
	if (!(ifmsg->ifi_index = if_nametoindex("vpeer1"))) {
		printErr("failed to get vpeer1");
	}
	//int nsfd = netns_get_fd("ns1");
	_nlmsg_put(nlmsg, IFLA_NET_NS_FD, &child_netns, sizeof(child_netns));

	if (_nlmsg_send(fd, nlmsg) != 0)
		exit(1);
	if (_nlmsg_recieve(fd) != 0) {
		close(fd);
		exit(1);
	}

	// set UP veth1 on the parent
	free(nlmsg);
	nlmsg = malloc(4096);
	memset(nlmsg, 0, 4096);
	nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	nlmsg->nlmsg_flags = NLM_F_ACK|NLM_F_REQUEST;
	nlmsg->nlmsg_type  = RTM_NEWLINK;
	nlmsg->nlmsg_seq   = time(NULL);

	ifmsg = (struct ifinfomsg *) NLMSG_DATA(nlmsg);
	ifmsg->ifi_family  = AF_UNSPEC;
	ifmsg->ifi_change |= IFF_UP;
	ifmsg->ifi_flags  |= IFF_UP;
	if (!(ifmsg->ifi_index = if_nametoindex("veth1"))) {
		printErr("failed to get veth1");
	}

	if (_nlmsg_send(fd, nlmsg) != 0)
		exit(1);
	if (_nlmsg_recieve(fd) != 0) {
		close(fd);
		exit(1);
	}

	close(fd);
	// enter in the child netns
	if (setns(child_netns, CLONE_NEWNET))
       printErr("setns");

	if ((fd = _nl_socket_init()) == 0)
		exit(1);
	// assign an ip address to vpeer1 (child)
	free(nlmsg);
	nlmsg = malloc(4096);
	memset(nlmsg, 0, 4096);
	nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	nlmsg->nlmsg_flags = NLM_F_ACK|NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL;
	nlmsg->nlmsg_type  = RTM_NEWADDR;
	nlmsg->nlmsg_seq   = time(NULL);

	struct ifaddrmsg *ifa_child;
	ifa_child = (struct ifaddrmsg *) NLMSG_DATA(nlmsg);
	ifa_child->ifa_prefixlen = atoi("24");
	if (!(ifa_child->ifa_index = if_nametoindex("vpeer1"))) {
		printErr("failed to get veth1");
	}
	ifa_child->ifa_family = AF_INET;
	ifa_child->ifa_scope = 0;

	struct in_addr addr_child;
	struct in_addr bcast_child;

	if (inet_pton(AF_INET, "172.16.1.2", &addr_child) < 0)
		exit(1);
	if (inet_pton(AF_INET, "172.16.1.255", &bcast_child) < 0)
		exit(1);

	_nlmsg_put(nlmsg, IFA_LOCAL,     &addr_child,  addrlen);
	_nlmsg_put(nlmsg, IFA_ADDRESS,   &addr_child,  addrlen);
	_nlmsg_put(nlmsg, IFA_BROADCAST, &bcast_child, addrlen);

	if (_nlmsg_send(fd, nlmsg) != 0)
		exit(1);
	if (_nlmsg_recieve(fd) != 0) {
		close(fd);
		exit(1);
	}

	// put vpeer1 UP
	free(nlmsg);
	nlmsg = malloc(4096);
	memset(nlmsg, 0, 4096);
	nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	nlmsg->nlmsg_flags = NLM_F_ACK|NLM_F_REQUEST;
	nlmsg->nlmsg_type  = RTM_NEWLINK;
	nlmsg->nlmsg_seq   = time(NULL);

	ifmsg = (struct ifinfomsg *) NLMSG_DATA(nlmsg);
	ifmsg->ifi_family  = AF_UNSPEC;
	ifmsg->ifi_change |= IFF_UP;
	ifmsg->ifi_flags  |= IFF_UP;
	if (!(ifmsg->ifi_index = if_nametoindex("vpeer1"))) {
		printf("failed to get veth1 name: %s\n", strerror(errno));
		exit(1);
	}

	if (_nlmsg_send(fd, nlmsg) != 0)
		exit(1);
	if (_nlmsg_recieve(fd) != 0) {
		close(fd);
		exit(1);
	}


	// put UP lo
	free(nlmsg);
	nlmsg = malloc(4096);
	memset(nlmsg, 0, 4096);
	nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	nlmsg->nlmsg_flags = NLM_F_ACK|NLM_F_REQUEST;
	nlmsg->nlmsg_type  = RTM_NEWLINK;
	nlmsg->nlmsg_seq   = time(NULL);

	ifmsg = (struct ifinfomsg *) NLMSG_DATA(nlmsg);
	ifmsg->ifi_family  = AF_UNSPEC;
	ifmsg->ifi_change |= IFF_UP;
	ifmsg->ifi_flags  |= IFF_UP;
	if (!(ifmsg->ifi_index = if_nametoindex("lo"))) {
		printErr("failed to get veth1");
	}

	if (_nlmsg_send(fd, nlmsg) != 0)
		exit(1);
	if (_nlmsg_recieve(fd) != 0) {
		close(fd);
		exit(1);
	}

	// default GW to the child
	free(nlmsg);
	nlmsg = malloc(4096);
	memset(nlmsg, 0, 4096);
	nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct rtmsg));
	nlmsg->nlmsg_flags = NLM_F_ACK|NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL;
	nlmsg->nlmsg_type  = RTM_NEWROUTE;
	nlmsg->nlmsg_seq   = time(NULL);

	struct rtmsg *rtm;
	rtm = (struct rtmsg *) NLMSG_DATA(nlmsg);

	rtm->rtm_family   = AF_INET;
	rtm->rtm_table    = RT_TABLE_MAIN;
	rtm->rtm_scope    = RT_SCOPE_UNIVERSE;
	rtm->rtm_protocol = RTPROT_BOOT;
	rtm->rtm_type     = RTN_UNICAST;
	rtm->rtm_dst_len  = 0;

	if (inet_pton(AF_INET, "172.16.1.1", &addr) < 0)
		exit(1);

	_nlmsg_put(nlmsg, RTA_GATEWAY, &addr, addrlen);

	if (_nlmsg_send(fd, nlmsg) != 0)
		exit(1);
	if (_nlmsg_recieve(fd) != 0) {
		close(fd);
		exit(1);
	}

	close(fd);

    if (setns(mynetns, CLONE_NEWNET))
        printErr("restore previous net namespace");


	if ((fd = _nl_socket_init()) == 0)
		exit(1);

	// nat and forwarding
	struct _rule *r = malloc(sizeof(struct _rule));
	memset(r, 0, sizeof(struct _rule));
	r->table = "nat";
	r->entry = "POSTROUTING";
	r->type  = "MASQUERADE";
	r->saddr = "172.16.1.0/24";
	r->oface = "eth0";
	_ipt_rule(r);

	r->saddr = 0;
	r->table = "filter";
	r->entry = "FORWARD";
	r->type  = "ACCEPT";
	r->oface = "eth0";
	r->iface = "veth0";
	_ipt_rule(r);

	r->oface = "veth0";
	r->iface = "eth0";
	_ipt_rule(r);

/*

    move_if_to_pid_netns(sock_fd, vpeer, child_netns);

    if (setns(child_netns, CLONE_NEWNET))
        printErr("setns");

    if_up(vpeer, vpeer_addr, netmask);

    if (setns(mynetns, CLONE_NEWNET))
        printErr("restore previous net namespace");
*/
    close(fd);
}