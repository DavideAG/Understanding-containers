#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <libiptc/libiptc.h>
#include <linux/netfilter/nf_nat.h>
#include <arpa/inet.h>
#include "helpers.h"


static void addattr_l(
        struct nlmsghdr *n, int maxlen, __u16 type,
        const void *data, __u16 datalen)
{
    __u16 attr_len = RTA_LENGTH(datalen);

    __u32 newlen = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(attr_len);
    if (newlen > maxlen)
        printf("cannot add attribute. size (%d) exceeded maxlen (%d)\n",
            newlen, maxlen);

    struct rtattr *rta;
    rta = NLMSG_TAIL(n);
    rta->rta_type = type;
    rta->rta_len = attr_len;
    if (datalen)
        memcpy(RTA_DATA(rta), data, datalen);

    n->nlmsg_len = newlen;
}

static struct rtattr *addattr_nest(
        struct nlmsghdr *n, int maxlen, __u16 type)
{
    struct rtattr *nest = NLMSG_TAIL(n);

    addattr_l(n, maxlen, type, NULL, 0);
    return nest;
}

static void addattr_nest_end(struct nlmsghdr *n, struct rtattr *nest)
{
    nest->rta_len = (void *)NLMSG_TAIL(n) - (void *)nest;
}

static ssize_t read_response(
        int fd, struct msghdr *msg, char **response)
{
    struct iovec *iov = msg->msg_iov;
    iov->iov_base = *response;
    iov->iov_len = MAX_PAYLOAD;

    ssize_t resp_len = recvmsg(fd, msg, 0);

    if (resp_len == 0)
        printf("EOF on netlink\n");

    if (resp_len < 0)
        printf("netlink receive error: %m\n");

    return resp_len;
}

static void check_response(int sock_fd)
{
    struct iovec iov;
    struct msghdr msg = {
            .msg_name = NULL,
            .msg_namelen = 0,
            .msg_iov = &iov,
            .msg_iovlen = 1
    };
    char *resp = malloc(MAX_PAYLOAD);

    ssize_t resp_len = read_response(sock_fd, &msg, &resp);

    struct nlmsghdr *hdr = (struct nlmsghdr *) resp;
    int nlmsglen = hdr->nlmsg_len;
    int datalen = nlmsglen - sizeof(*hdr);

    // Did we read all data?
    if (datalen < 0 || nlmsglen > resp_len) {
        if (msg.msg_flags & MSG_TRUNC)
            printf("received truncated message\n");

        printf("malformed message: nlmsg_len=%d\n", nlmsglen);
    }

    // Was there an error?
    if (hdr->nlmsg_type == NLMSG_ERROR) {
        struct nlmsgerr *err = (struct nlmsgerr *) NLMSG_DATA(hdr);

        if (datalen < sizeof(struct nlmsgerr))
            fprintf(stderr, "ERROR truncated!\n");

        if(err->error) {
            errno = -err->error;
            printf("RTNETLINK: %m\n");
        }
    }

    free(resp);
}

int create_socket(int domain, int type, int protocol)
{
    int sock_fd = socket(domain, type, protocol);
    if (sock_fd < 0)
        printf("cannot open socket: %m\n");

    return sock_fd;
}

static void send_nlmsg(int sock_fd, struct nlmsghdr *n)
{
    struct iovec iov = {
            .iov_base = n,
            .iov_len = n->nlmsg_len
    };

    struct msghdr msg = {
            .msg_name = NULL,
            .msg_namelen = 0,
            .msg_iov = &iov,
            .msg_iovlen = 1
    };

    n->nlmsg_seq++;

    ssize_t status = sendmsg(sock_fd, &msg, 0);
    if (status < 0)
        printf("cannot talk to rtnetlink: %m\n");

    check_response(sock_fd);
}

int get_netns_fd(int pid)
{
    char path[256];
    sprintf(path, "/proc/%d/ns/net", pid);

    int fd = open(path, O_RDONLY);

    if (fd < 0)
        printf("cannot read netns file %s: %m\n", path);

    return fd;
}

void if_up(char *ifname, char *ip, char *netmask)
{
    int sock_fd = create_socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, ifname, strlen(ifname));

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(struct sockaddr_in));
    saddr.sin_family = AF_INET;
    saddr.sin_port = 0;

    char *p = (char *) &saddr;

    saddr.sin_addr.s_addr = inet_addr(ip);
    memcpy(((char *)&(ifr.ifr_addr)), p, sizeof(struct sockaddr));
    if (ioctl(sock_fd, SIOCSIFADDR, &ifr))
        printf("cannot set ip addr %s, %s: %m\n", ifname, ip);

    saddr.sin_addr.s_addr = inet_addr(netmask);
    memcpy(((char *)&(ifr.ifr_addr)), p, sizeof(struct sockaddr));
    if (ioctl(sock_fd, SIOCSIFNETMASK, &ifr))
        printf("cannot set netmask for addr %s, %s: %m\n", ifname, netmask);

    ifr.ifr_flags |= IFF_UP | IFF_BROADCAST |
                     IFF_RUNNING | IFF_MULTICAST;
    if (ioctl(sock_fd, SIOCSIFFLAGS, &ifr))
        printf("cannot set flags for addr %s, %s: %m\n", ifname, ip);

    close(sock_fd);
}

void create_veth(int sock_fd, char *ifname, char *peername)
{
    // ip link add veth0 type veth peer name veth1
    __u16 flags =
            NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK;
    struct nl_req req = {
            .n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
            .n.nlmsg_flags = flags,
            .n.nlmsg_type = RTM_NEWLINK,
            .i.ifi_family = PF_NETLINK,
    };
    struct nlmsghdr *n = &req.n;
    int maxlen = sizeof(req);

    addattr_l(n, maxlen, IFLA_IFNAME, ifname, strlen(ifname) + 1);

    struct rtattr *linfo =
            addattr_nest(n, maxlen, IFLA_LINKINFO);
    addattr_l(&req.n, sizeof(req), IFLA_INFO_KIND, "veth", 5);

    struct rtattr *linfodata =
            addattr_nest(n, maxlen, IFLA_INFO_DATA);

    struct rtattr *peerinfo =
            addattr_nest(n, maxlen, VETH_INFO_PEER);
    n->nlmsg_len += sizeof(struct ifinfomsg);
    addattr_l(n, maxlen, IFLA_IFNAME, peername, strlen(peername) + 1);
    addattr_nest_end(n, peerinfo);

    addattr_nest_end(n, linfodata);
    addattr_nest_end(n, linfo);

    send_nlmsg(sock_fd, n);
}

void move_if_to_pid_netns(int sock_fd, char *ifname, int netns)
{
    // ip link set veth1 netns coke
    struct nl_req req = {
            .n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
            .n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK,
            .n.nlmsg_type = RTM_NEWLINK,
            .i.ifi_family = PF_NETLINK,
    };

    addattr_l(&req.n, sizeof(req), IFLA_NET_NS_FD, &netns, 4);
    addattr_l(&req.n, sizeof(req), IFLA_IFNAME,
              ifname, strlen(ifname) + 1);
    send_nlmsg(sock_fd, &req.n);
}

// returns 0 on success and -1 on failure
int drop_root_privileges(void)
{
	gid_t gid;
	uid_t uid;

	// no need to "drop" the privileges that you don't have in the first place!
	if (getuid() != 0) {
		return 0;
	}

	// when your program is invoked with sudo, getuid() will return 0 and you
	// won't be able to drop your privileges
	if ((uid = getuid()) == 0) {
		const char *sudo_uid = secure_getenv("SUDO_UID");
		if (sudo_uid == NULL) {
			printf("environment variable `SUDO_UID` not found\n");
			return -1;
		}
		errno = 0;
		uid = (uid_t) strtoll(sudo_uid, NULL, 10);
		if (errno != 0) {
			perror("under-/over-flow in converting `SUDO_UID` to integer");
			return -1;
		}
	}

	// again, in case your program is invoked using sudo
	if ((gid = getgid()) == 0) {
		const char *sudo_gid = secure_getenv("SUDO_GID");
		if (sudo_gid == NULL) {
			printf("environment variable `SUDO_GID` not found\n");
			return -1;
		}
		errno = 0;
		gid = (gid_t) strtoll(sudo_gid, NULL, 10);
		if (errno != 0) {
			perror("under-/over-flow in converting `SUDO_GID` to integer");
			return -1;
		}
	}

	
	if (setgid(gid) != 0) {
		perror("setgid");
		return -1;
	}
	if (setuid(uid) != 0) {
		perror("setgid");
		return -1;
	}

	// change your directory to somewhere else, just in case if you are in a
	// root-owned one (e.g. /root)
	if (chdir("/") != 0) {
		perror("chdir");
		return -1;
	}

	// check if we successfully dropped the root privileges
	if (setuid(0) == 0 || seteuid(0) == 0) {
		printf("could not drop root privileges!\n");
		return -1;
	}

	return 0;
}

// write the child entrypoint command
void get_child_entrypoint(int optind,
            char **arguments,
            size_t size,
            char ***child_entrypoint)
{
    int index;

    (*child_entrypoint) = (char **) malloc(size - optind);
    for (index = optind; index < size; ++index)
    {
        (*child_entrypoint)[index - optind] = strdup(arguments[index]);
    }
}

int
masquerade()
{
    struct xtc_handle *h = iptc_init("nat");
	int           result = 0;
	if (!h) {
		printf( "error condition  %s\n", iptc_strerror(errno));
		return -1;
	}

	unsigned int targetOffset = XT_ALIGN(sizeof(struct ipt_entry));
	unsigned int totalLen     = targetOffset + XT_ALIGN(sizeof(struct xt_entry_target))
		+ XT_ALIGN(sizeof(struct nf_nat_ipv4_multi_range_compat));

	struct ipt_entry* e = (struct ipt_entry *)calloc(1, totalLen);
	if(e == NULL) {
		printf("calloc failure :%s\n", strerror(errno));
		return -1;
	}

	e->target_offset = targetOffset;
	e->next_offset   = totalLen;
	strncpy(e->ip.outiface, "eth0", strlen("eth0") + 1);
	unsigned int ip, mask;
	inet_pton (AF_INET, "10.1.1.0", &ip);
	inet_pton (AF_INET, "255.255.255.0", &mask);
	e->ip.src.s_addr  = ip;
	e->ip.smsk.s_addr = mask;

	struct xt_entry_target* target = (struct xt_entry_target  *) e->elems;
	target->u.target_size          = XT_ALIGN(sizeof(struct xt_entry_target))
		+ XT_ALIGN(sizeof(struct nf_nat_ipv4_multi_range_compat));

	strncpy(target->u.user.name, "MASQUERADE", strlen("MASQUERADE") + 1);
	target->u.user.revision = 0;
	struct nf_nat_ipv4_multi_range_compat* masquerade = (struct nf_nat_ipv4_multi_range_compat  *) target->data;
	masquerade->rangesize   = 1;

	if (!iptc_append_entry("POSTROUTING", e, h)) {
		printf("iptc_append_entry::Error insert/append entry: %s\n", iptc_strerror(errno));
		result = -1;
		goto end;
	}
	if (!iptc_commit(h)) {
		printf("iptc_commit::Error commit: %s\n", iptc_strerror(errno));
		result = -1;
	}

	end:
		free(e);
		iptc_free(h);
		return result;
}

int
forward()
{
    struct xtc_handle *h = iptc_init("filter");
	int result = 0;
	if (!h) { printf( "error condition  %s\n", iptc_strerror(errno)); return -1;}

	unsigned int targetOffset =  XT_ALIGN(sizeof(struct ipt_entry));
	unsigned int totalLen     = targetOffset + XT_ALIGN(sizeof(struct xt_standard_target));

	struct ipt_entry* e = (struct ipt_entry *)calloc(1, totalLen);
	if (e == NULL) {
		printf("calloc failure :%s\n", strerror(errno));
		return -1;
	}

	e->target_offset = targetOffset;
	e->next_offset = totalLen;

	strcpy(e->ip.outiface, "wlp58s0");
	strcpy(e->ip.iniface, "veth0");

	struct xt_standard_target* target = (struct xt_standard_target  *) e->elems;
	target->target.u.target_size = XT_ALIGN(sizeof(struct xt_standard_target));
	strncpy(target->target.u.user.name, "ACCEPT", strlen("ACCEPT") + 1);
	target->target.u.user.revision = 0;
	target->verdict                = -NF_ACCEPT - 1;

	if (iptc_append_entry("FORWARD", e, h) == 0) {
		printf("iptc_append_entry::Error insert/append entry: %s\n", iptc_strerror(errno));
		result = -1;
		goto end;
	}
	if (iptc_commit(h) == 0) {
		printf("iptc_commit::Error commit: %s\n", iptc_strerror(errno));
		result = -1;
	}

	end:
		free(e);
		iptc_free(h);
		return result;
}



int _nl_socket_init(void)
{
	int fd = 0;
	if ((fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0) {
		fprintf(stderr, "failed to get socket: %s\n", strerror(errno));
		return 0;
	}

	int sndbuf = 32768;
	int rcvbuf = 32768;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
		&sndbuf, sizeof(sndbuf)) < 0) {
		fprintf(stderr, "failed to set send buffer: %s\n", strerror(errno));
		return 0;
	}
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
		&rcvbuf,sizeof(rcvbuf)) < 0) {
		fprintf(stderr, "failed to set recieve buffer: %s\n", strerror(errno));
		return 0;
	}
	struct sockaddr_nl *sa = malloc(sizeof(struct sockaddr_nl));
	memset(sa, 0, sizeof(struct sockaddr_nl));
	sa->nl_family = AF_NETLINK;
	sa->nl_groups = 0;
	if (bind(fd, (struct sockaddr *) sa, sizeof(struct sockaddr)) < 0) {
		fprintf(stderr, "failed to bind socket: %s\n", strerror(errno));
		return 0;
	}

	return fd;
}


void _nlmsg_put(struct nlmsghdr *nlmsg, int type, void *data, size_t len)
{
	struct rtattr *rta;
	size_t  rtalen = RTA_LENGTH(len);
	rta = NLMSG_TAIL(nlmsg);
	rta->rta_type = type;
	rta->rta_len  = rtalen;
	if (data)
		memcpy(RTA_DATA(rta), data, len);
	nlmsg->nlmsg_len = NLMSG_ALIGN(nlmsg->nlmsg_len) + RTA_ALIGN(rtalen);
}

int _nlmsg_send(int fd, struct nlmsghdr *nlmsg)
{
	struct sockaddr_nl *sa = malloc(sizeof(struct sockaddr_nl));
	memset(sa, 0, sizeof(struct sockaddr_nl));
	sa->nl_family = AF_NETLINK;

	struct iovec  iov = { nlmsg, nlmsg->nlmsg_len };
	struct msghdr msg = { sa, sizeof(*sa), &iov, 1, NULL, 0, 0 };

	if (sendmsg(fd, &msg, 0) < 0) {
		fprintf(stderr, "failed to get socket: %s\n", strerror(errno));
		return 1;
	}

	return 0;
}

int _nlmsg_recieve(int fd)
{
	struct sockaddr_nl *sa = malloc(sizeof(struct sockaddr_nl));
	memset(sa, 0, sizeof(struct sockaddr_nl));
	sa->nl_family = AF_NETLINK;

	int len = 4096;
	char buf[len];
	struct iovec  iov = { buf, len };
	struct msghdr msg = { sa, sizeof(*sa), &iov, 1, NULL, 0, 0 };

	recvmsg(fd, &msg, 0);
	struct nlmsghdr *ret = (struct nlmsghdr*)buf;
	if (ret->nlmsg_type == NLMSG_ERROR) {
		struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(ret);
		if (err->error < 0) {
			fprintf(stderr, "recieve error: %d\n", err->error);
			return 1;
		}
	} else {
		fprintf(stderr, "invalid recieve type\n");
		return 1;
	}

	return 0;
}

int _ipt_rule(struct _rule *rule)
{
	if (!rule->table)
		return -1;
	if (!rule->type)
		return -1;
	if (!rule->entry)
		return -1;
	struct xtc_handle *h = iptc_init(rule->table);
	int result = 0;

	if (!h) {
		printf( "error condition  %s\n", iptc_strerror(errno));
		return -1;
	}

	unsigned int targetOffset =  XT_ALIGN(sizeof(struct ipt_entry));
	unsigned int totalLen     = targetOffset + XT_ALIGN(sizeof(struct xt_standard_target));

	if (strcmp(rule->type, "MASQUERADE") == 0)
		totalLen +=  XT_ALIGN(sizeof(struct nf_nat_ipv4_multi_range_compat));

	struct ipt_entry* e = (struct ipt_entry *)calloc(1, totalLen);
	if (e == NULL) {
		printf("calloc failure :%s\n", strerror(errno));
		return -1;
	}

	e->target_offset = targetOffset;
	e->next_offset   = totalLen;

	if (rule->oface)
		strncpy(e->ip.outiface, rule->oface, strlen(rule->oface) + 1);
	if (rule->iface)
		strncpy(e->ip.iniface,  rule->iface, strlen(rule->iface) + 1);

	if (rule->saddr) {
		struct _addr_t *addr = _init_addr(rule->saddr);
		e->ip.src.s_addr  = addr->addr;
		e->ip.smsk.s_addr = addr->mask;
		_free_addr(addr);
	}
	if (rule->daddr) {
		struct _addr_t *addr = _init_addr(rule->daddr);
		e->ip.dst.s_addr  = addr->addr;
		e->ip.dmsk.s_addr = addr->mask;
		_free_addr(addr);
	}

	if (strcmp(rule->type, "MASQUERADE") == 0) {
		struct xt_entry_target* target = (struct xt_entry_target  *) e->elems;
		target->u.target_size          = XT_ALIGN(sizeof(struct xt_entry_target))
			+ XT_ALIGN(sizeof(struct nf_nat_ipv4_multi_range_compat));
		strncpy(target->u.user.name, rule->type, strlen(rule->type) + 1);
		target->u.user.revision = 0;
		struct nf_nat_ipv4_multi_range_compat* masquerade = (struct nf_nat_ipv4_multi_range_compat  *) target->data;
		masquerade->rangesize   = 1;
	} else {
		struct xt_standard_target* target  = (struct xt_standard_target  *) e->elems;
		target->target.u.target_size = XT_ALIGN(sizeof(struct xt_standard_target));
		strncpy(target->target.u.user.name, rule->type, strlen(rule->type) + 1);
		target->target.u.user.revision = 0;
		target->verdict                = -NF_ACCEPT - 1;
	}

	if (iptc_append_entry(rule->entry, e, h) == 0) {
		printf("iptc_append_entry::Error insert/append entry: %s\n", iptc_strerror(errno));
		result = -1;
		goto end;
	}
	if (iptc_commit(h) == 0) {
		printf("iptc_commit::Error commit: %s\n", iptc_strerror(errno));
		result = -1;
	}

	end:
		free(e);
		iptc_free(h);
		return result;
}

struct _addr_t *  _init_addr(const char *ip)
{
	struct _addr_t *addr = malloc(sizeof(struct _addr_t));
	memset(addr, 0, sizeof(struct _addr_t));

	char *readdr, *prefix, *ptr;

	ptr = strrchr(ip, '/');
	prefix = strdup(ptr); prefix++;
	unsigned long mask = (0xFFFFFFFF << (32 - (unsigned int) atoi(prefix))) & 0xFFFFFFFF;

	readdr = strdup(ip);
	readdr[strlen(ip) - strlen(ptr)] = '\0';
	unsigned int i;
	inet_pton(AF_INET, readdr, &i);
	addr->addr = i;
	addr->mask = htonl(mask);
	return addr;
}

void _free_addr(struct _addr_t *addr)
{
	free(addr);
}