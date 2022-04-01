#ifndef INTERFACE_H
#define INTERFACE_H 1

#include <sys/types.h>
#include <netinet/in.h>
#include <sysrepo.h>

#include "list.h"
#include "shash.h"
#include "svec.h"
#include "sset.h"

struct interface {
    char        *name;
    int          index;
    char         hw_addr[24];
    unsigned int flags; /* Flags from SIOCGIFFLAGS see netdevice(7) */
    char        *type;
    uint64_t     speed;
    int          vlan_id;
    int          pvid;
    char        *master_name;
};

struct address {
    struct list_node node;
    bool             is_ipv4;
    char             addr[INET6_ADDRSTRLEN];
    char             netmask[INET6_ADDRSTRLEN];
    uint8_t          prefix;
};

struct ip {
    char *name;
    bool  is_up;
    int   mtu;
    struct list_node addresses;
};

struct interface *interface_create();
void interface_destroy(struct interface *intf);

void collect_interfaces(struct shash *interfaces);
void save_interface_running(struct interface *intf, sr_session_ctx_t *session);
void save_interface_operational(struct interface *interface,
        sr_session_ctx_t *session);

bool collect_ips(struct shash *interfaces, struct shash *ips);
void save_ips(struct shash *ips, sr_session_ctx_t *session);
void update_ips(struct shash *interfaces, sr_session_ctx_t *session);

void update_interfaces_speed(struct shash *interfaces, sr_session_ctx_t *session);

void interface_statistics_provider(sr_session_ctx_t *session, struct lyd_node **parent);
void interface_oper_status_provider(sr_session_ctx_t *session, struct lyd_node **parent);

struct sset *get_interface_names();
void destroy_interface_names();
#endif /* interface.h */