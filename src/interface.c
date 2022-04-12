//
// Created by sgk on 2021/1/21.
//

#include "interface.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>

#include <linux/rtnetlink.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <linux/if.h>
#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/link/vlan.h>
#include <netlink/route/link/bridge.h>
#include <sysrepo.h>

#include "log.h"
#include "dynamic-string.h"
#include "sset.h"
#include "utils.h"

static struct sset *interface_names = NULL;

inline struct interface *interface_create()
{
    struct interface *intf = (struct interface*)calloc(1, sizeof(struct interface));
    memset(intf, 0, sizeof(struct interface));
    return intf;
}

void interface_destroy(struct interface *intf)
{
    if (intf == NULL) {
        return;
    }

    if (intf->name != NULL) {
        free(intf->name);
    }

    if (intf->type != NULL) {
        free(intf->type);
    }

    if (intf->master_name != NULL) {
        free(intf->master_name);
    }

    free(intf);
}

struct ip *ip_create()
{
    struct ip *ip = NULL;
    ip = (struct ip*)calloc(1, sizeof(struct ip));
    ip->mtu = -1;
    list_init(&ip->addresses);
    return ip;
}

void ip_destory(struct ip *ip)
{
    struct address *addr, *addr_next;

    if (NULL != ip) {
        return;
    }

    if (NULL != ip->name) {
        free(ip->name);
    }

    LIST_FOR_EACH_SAFE(addr, addr_next, node, &ip->addresses) {
        list_remove(&addr->node);
        free(addr);
    }

    free(ip);
}

void get_interface_mtu_and_flags(char *name, int *mtu, int *flags)
{
    int sockfd;
    struct ifreq ifr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        log_error("Create socket failed");
        return;
    }

    strcpy(ifr.ifr_name, name);
    if (ioctl(sockfd, SIOCGIFMTU, &ifr) == 0) {
        *mtu = ifr.ifr_mtu;
    }

    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) == 0) {
        *flags = ifr.ifr_flags;
    }

    close(sockfd);

    return;
}

static void get_interface_speed(char *interface_name, unsigned int *speed)
{
    int sockfd;
    struct ifreq ifr;
    struct ethtool_cmd edata;
    int rc;

    sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sockfd < 0) {
        log_error("Open socket failed, when get interface-%s's speed", interface_name);
        *speed = -1;
        return;
    }

    strncpy(ifr.ifr_name, interface_name, sizeof(ifr.ifr_name));
    ifr.ifr_data = (__caddr_t) &edata;

    edata.cmd = ETHTOOL_GSET;

    rc = ioctl(sockfd, SIOCETHTOOL, &ifr);
    if (rc < 0) {
        log_error("ioctl failed when get interface-%s's speed", interface_name);
        *speed =  -1;
        return;
    }

    *speed = ethtool_cmd_speed(&edata);
    close(sockfd);
}

void collect_interfaces(struct shash *interfaces)
{
    struct if_nameindex *if_ni, *idx_p;
    struct nl_sock *sk = NULL;
    struct nl_cache *cache = NULL;
    struct rtnl_link *link = NULL;
    int status;
    unsigned int flags = 0, speed = 0;

    if_ni = if_nameindex();

    sk = nl_socket_alloc();
    if (sk == NULL) {
        log_error("Allocate nl socket failed");
        return;
    }

    status = nl_connect(sk, NETLINK_ROUTE);
    if (status != 0) {
        log_error("Connect to nl failed");
        nl_socket_free(sk);
        return;
    }

    status = rtnl_link_alloc_cache(sk, AF_UNSPEC, &cache);
    if (status != 0) {
        log_error("Alloc rtnl link cache failed");
        goto cleanup;
    }

    for (idx_p = if_ni;
         idx_p != NULL && idx_p->if_index != 0 && idx_p->if_name != NULL;
         idx_p++)
    {
        if (strstr(idx_p->if_name, "eno") == NULL && strstr(idx_p->if_name, "swp") == NULL) {
            continue;
        }

        link = rtnl_link_get_by_name(cache, idx_p->if_name);
        if (link != NULL && rtnl_link_get_arptype(link) != 772) {
            char *type = rtnl_link_get_type(link);
            flags = rtnl_link_get_flags(link);
            //if (flags & IFF_LOOPBACK || (type != NULL && strcmp(type, "vlan") != 0 && strcmp(type, "veth") != 0)) {
            //    rtnl_link_put(link);
            //    continue;
            //}

            struct interface *intf = interface_create();
            intf->name = strdup(idx_p->if_name);
            intf->index = rtnl_link_get_ifindex(link);

            struct nl_addr *addr = rtnl_link_get_addr(link);
            nl_addr2str(addr, intf->hw_addr, sizeof(intf->hw_addr) - 1);

            intf->flags = flags;

            if (type != NULL) {
                intf->type = strdup(type);
            }

            get_interface_speed(idx_p->if_name, &speed);
            if (speed != -1) {
                intf->speed = (uint64_t)speed * 1000000ul;
            } else {
                intf->speed = -1;
            }

            int master = rtnl_link_get_master(link);
            if (master != 0) {
                struct rtnl_link *tmp = rtnl_link_get(cache, master);
                intf->master_name = strdup(rtnl_link_get_name(tmp));
                rtnl_link_put(tmp);
            }

            if (rtnl_link_is_vlan(link)) {
                intf->vlan_id = rtnl_link_vlan_get_id(link);
            }

            if (rtnl_link_is_bridge(link)) {
                intf->pvid = rtnl_link_bridge_pvid(link);
            }

            shash_add(interfaces, intf->name, intf);
        }

        rtnl_link_put(link);
    }

cleanup:
    if (cache != NULL) {
        nl_cache_free(cache);
    }

    if (sk != NULL) {
        nl_close(sk);
        nl_socket_free(sk);
    }
}

bool collect_ips(struct shash *interfaces, struct shash *ips)
{
    struct ifaddrs *addrs, *addr;
    struct ip  *ip = NULL;
    struct address *address = NULL;
    int flags = 0;

    if (getifaddrs(&addrs) != 0) {
        log_error("Get interface addresses failed");
        return false;
    }

    addr = addrs;
    while (addr) {
        if (shash_find(interfaces, addr->ifa_name) == NULL) {
            addr = addr->ifa_next;
            continue;
        }

        if (addr->ifa_addr && addr->ifa_addr->sa_family == AF_INET) {
            if ((ip = (struct ip*)shash_find_data(ips, addr->ifa_name)) == NULL) {
                ip = ip_create();
                ip->name = strdup(addr->ifa_name);
                flags = 0;
                get_interface_mtu_and_flags(addr->ifa_name, &ip->mtu, &flags);
                ip->is_up = flags & IFF_UP;
                shash_add(ips, addr->ifa_name, (void*)ip);
            }

            address = (struct address *)calloc(1, sizeof(struct address));
            address->is_ipv4 = true;
            inet_ntop(AF_INET, &((struct sockaddr_in *)addr->ifa_addr)->sin_addr,
                      address->addr, sizeof(address->addr));
            inet_ntop(AF_INET, &((struct sockaddr_in *)addr->ifa_netmask)->sin_addr,
                      address->netmask, sizeof(address->netmask));
            address->prefix = netmask_prefix(addr->ifa_netmask);

            list_push_back(&ip->addresses, &address->node);
        } else if (addr->ifa_addr && addr->ifa_addr->sa_family == AF_INET6) {
            if ((ip = (struct ip*)shash_find_data(ips, addr->ifa_name)) == NULL) {
                ip = ip_create();
                ip->name = strdup(addr->ifa_name);
                flags = 0;
                get_interface_mtu_and_flags(addr->ifa_name, &ip->mtu, &flags);
                ip->is_up = flags & IFF_UP;
                shash_add(ips, addr->ifa_name, (void*)ip);
            }

            address = (struct address *)calloc(1, sizeof(struct address));
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)addr->ifa_addr)->sin6_addr,
                      address->addr, sizeof(address->addr));
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)addr->ifa_netmask)->sin6_addr,
                      address->netmask, sizeof(address->netmask));
            address->prefix = netmask_prefix(addr->ifa_netmask);

            list_push_back(&ip->addresses, &address->node);
        }

        addr = addr->ifa_next;
    }

    freeifaddrs(addrs);
    return true;
}

void save_ips(struct shash *ips, sr_session_ctx_t *session)
{
    struct ip *ip = NULL;
    struct shash_node *node;
    struct address *address = NULL;
    struct ds xpath = DS_EMPTY_INITIALIZER;
    int rc = SR_ERR_OK;
    sr_val_t val = { 0 };

    //rc = sr_delete_item(session, "/ietf-interfaces:interfaces/interface/ietf-ip:ipv4", SR_EDIT_DEFAULT);
    //if (rc != SR_ERR_OK) {
    //    fprintf(stderr, "Delete all interface ipv4 from sysrepo failed: %s\n", sr_strerror(rc));
    //}

    //rc = sr_delete_item(session, "/ietf-interfaces:interfaces/interface/ietf-ip:ipv6", SR_EDIT_DEFAULT);
    //if (rc != SR_ERR_OK) {
    //    fprintf(stderr, "Delete all interface ipv6 from sysrepo failed: %s\n", sr_strerror(rc));
    //}

    //rc = sr_apply_changes(session, 0, 0);
    //if (SR_ERR_OK != rc) {
    //    fprintf(stderr, "delete ip failed: %s\n", sr_strerror(rc));
    //}

    SHASH_FOR_EACH(node, ips) {
        struct address *addr = NULL;
        ip = (struct ip*)node->data;

        ds_clear(&xpath);
        ds_put_format(&xpath, "/ietf-interfaces:interfaces/interface[name='%s']/ietf-ip:ipv4/mtu", ip->name);
        val.xpath = ds_cstr(&xpath);
        val.type = SR_UINT16_T;
        val.data.uint16_val = ip->mtu;
        rc = sr_set_item(session, ds_cstr(&xpath), &val, SR_EDIT_DEFAULT);
        if (rc != SR_ERR_OK) {
            fprintf(stderr, "set path-%s failed: %s\n", ds_cstr(&xpath), sr_strerror(rc));
        }


        LIST_FOR_EACH(addr, node, &ip->addresses) {
            if (addr->is_ipv4) {
                ds_clear(&xpath);
                ds_put_format(&xpath,
                              "/ietf-interfaces:interfaces/interface[name='%s']/ietf-ip:ipv4/address[ip='%s']/prefix-length",
                              ip->name,
                              addr->addr);
                val.type = SR_UINT8_T;
                val.data.uint8_val = addr->prefix;
                rc = sr_set_item(session, ds_cstr(&xpath), &val, 0);
                if (SR_ERR_OK != rc) {
                    log_error("Set prefix-length failed: %s", sr_strerror(rc));
                }
            } else {
                ds_clear(&xpath);
                ds_put_format(&xpath,
                              "/ietf-interfaces:interfaces/interface[name='%s']/ietf-ip:ipv6/address[ip='%s']/prefix-length",
                              ip->name,
                              addr->addr);
                val.type = SR_UINT8_T;
                val.data.uint8_val = addr->prefix;
                rc = sr_set_item(session, ds_cstr(&xpath), &val, 0);
                if (SR_ERR_OK != rc) {
                    log_error("Set prefix-length failed: %s", sr_strerror(rc));
                }
            }

            /* commit the changes */
            rc = sr_apply_changes(session, 0);
            if (SR_ERR_OK != rc) {
                log_error("Set prefix-length apply failed", sr_strerror(rc));
            }
        }
    }

cleanup:
    return;
}

void save_interface_running(struct interface *intf, sr_session_ctx_t *session)
{
    int rc = SR_ERR_OK;
    struct ds path = DS_EMPTY_INITIALIZER;
    sr_val_t val = { 0 };

    ds_put_format(&path, "/ietf-interfaces:interfaces/interface[name='%s']/type",
                  intf->name);
    val.type = SR_IDENTITYREF_T;
    val.data.string_val = "iana-if-type:ethernetCsmacd";
    rc = sr_set_item(session, ds_cstr(&path), &val, 0);
    if (rc != SR_ERR_OK) {
        log_error("Set %s=%s failed: %s", ds_cstr(&path), val.data.string_val,
                  sr_strerror(rc));
    }

    rc = sr_apply_changes(session, 0);
    if (rc != SR_ERR_OK) {
        log_error("Set interface's type, apply failed: %s", sr_strerror(rc));
        sr_discard_changes(session);
    }

    ds_destroy(&path);
}

void save_interface_operational(struct interface *interface,
        sr_session_ctx_t *session)
{
    int rc = SR_ERR_OK;
    struct ds path = DS_EMPTY_INITIALIZER;
    sr_val_t val = {0};

    ds_put_format(&path,
                  "/ietf-interfaces:interfaces/interface[name='%s']/admin-status",
                  interface->name);
    val.type = SR_ENUM_T;
    val.data.enum_val = "up";
    rc = sr_set_item(session, ds_cstr(&path), &val, 0);
    if (rc != SR_ERR_OK) {
        log_error("Set %s=%s failed: %s", ds_cstr(&path), val.data.enum_val);
    }

    ds_clear(&path);
    ds_put_format(&path,
                  "/ietf-interfaces:interfaces/interface[name='%s']/phys-address",
                  interface->name);
    rc = sr_set_item_str(session, ds_cstr(&path), interface->hw_addr, 0, 0);
    if (rc != SR_ERR_OK) {
        log_error("Set %s=%s failed: %s", ds_cstr(&path),
                  interface->hw_addr, sr_strerror(rc));
    }

    ds_clear(&path);
    ds_put_format(&path,
                  "/ietf-interfaces:interfaces/interface[name='%s']/if-index",
                  interface->name);
    val.type = SR_INT32_T;
    val.data.int32_val = interface->index;
    rc = sr_set_item(session, ds_cstr(&path), &val, 0);
    if (rc != SR_ERR_OK) {
        log_error("Set %s=%d failed: %s", ds_cstr(&path),
                  interface->index, sr_strerror(rc));
    }

    ds_clear(&path);
    ds_put_format(&path,
                  "/ietf-interfaces:interfaces/interface[name='%s']/speed",
                  interface->name);
    val.type = SR_UINT64_T;
    val.data.uint64_val = interface->speed;
    rc = sr_set_item(session, ds_cstr(&path), &val, 0);
    if (rc != SR_ERR_OK) {
        log_error("Set %s=%s failed: %s", ds_cstr(&path), interface->speed, sr_strerror(rc));
    }

    rc = sr_apply_changes(session, 0);
    if (rc != SR_ERR_OK) {
        log_error("Set interface's speed apply failed: %s", sr_strerror(rc));
        sr_discard_changes(session);
    }

    ds_destroy(&path);
}

static inline void add_new_path(
        const char *format, const char *interface_name, const char *node_name,
        struct lyd_node *parent, struct rtnl_link *link, rtnl_link_stat_id_t id)
{
    struct ds path = DS_EMPTY_INITIALIZER;
    char value_str[24] = {0};

    ds_put_format(&path, format, interface_name, node_name);
    sprintf(value_str, "%lu", rtnl_link_get_stat(link, id));
    lyd_new_path(parent, NULL, ds_cstr(&path), value_str, 0, 0);

    ds_destroy(&path);
}

void interface_statistics_provider(sr_session_ctx_t *session, struct lyd_node **parent)
{
    struct sset *names = NULL;
    const char *name = NULL;
    struct nl_sock *sk = NULL;
    struct nl_cache *cache = NULL;
    struct rtnl_link *link = NULL;
    int status;
    struct ds path = DS_EMPTY_INITIALIZER;
    const char *format = "/ietf-interfaces:interfaces/interface[name='%s']/statistics/%s";
    char *current = NULL;
    bool start = true;
    const struct ly_ctx *ly_ctx;

    ly_ctx = sr_acquire_context(sr_session_get_connection(session));

    sk = nl_socket_alloc();
    if (sk == NULL) {
        log_error("Allocate nl socket failed");
        return;
    }

    status = nl_connect(sk, NETLINK_ROUTE);
    if (status != 0) {
        log_error("Connect to nl failed");
        nl_socket_free(sk);
        return;
    }

    status = rtnl_link_alloc_cache(sk, AF_UNSPEC, &cache);
    if (status != 0) {
        log_error("Alloc rtnl link cache failed");
        goto cleanup;
    }

    names = get_interface_names();
    SSET_FOR_EACH(name, names) {
        link = rtnl_link_get_by_name(cache, name);
        ds_clear(&path);
        ds_put_format(&path, format, name, "discontinuity-time");
        current = get_iso8601_time();
        if (link != NULL) {
            if (start) {
//                *parent = lyd_new_path(NULL, sr_get_context(sr_session_get_connection(session)),
//                                       ds_cstr(&path), current, 0, 0);
                lyd_new_path(NULL, ly_ctx, ds_cstr(&path), current, 0, parent);
                start = false;
            } else {
                lyd_new_path(*parent, NULL, ds_cstr(&path), current, 0, NULL);
            }
            free(current);

            add_new_path(format, name, "in-octets", *parent, link, RTNL_LINK_RX_BYTES);
            add_new_path(format, name, "in-unicast-pkts", *parent, link, RTNL_LINK_RX_PACKETS);
            add_new_path(format, name, "in-errors", *parent, link, RTNL_LINK_RX_ERRORS);
            add_new_path(format, name, "in-discards", *parent, link, RTNL_LINK_RX_DROPPED);
            add_new_path(format, name, "out-octets", *parent, link, RTNL_LINK_TX_BYTES);
            add_new_path(format, name, "out-unicast-pkts", *parent, link, RTNL_LINK_TX_PACKETS);
            add_new_path(format, name, "out-errors", *parent, link, RTNL_LINK_TX_ERRORS);
            add_new_path(format, name, "out-discards", *parent, link, RTNL_LINK_TX_DROPPED);
        }

        rtnl_link_put(link);
    }

    ds_destroy(&path);

cleanup:
    sr_release_context(sr_session_get_connection(session));

    if (cache != NULL) {
        nl_cache_free(cache);
    }

    if (sk != NULL) {
        nl_close(sk);
        nl_socket_free(sk);
    }
}

void interface_oper_status_provider(sr_session_ctx_t *session, struct lyd_node **parent)
{
    struct sset *names = NULL;
    const char *name = NULL;
    char *oper_state = "";
    struct nl_sock *sk = NULL;
    struct nl_cache *cache = NULL;
    struct rtnl_link *link = NULL;
    int status;
    struct ds path = DS_EMPTY_INITIALIZER;
    bool start = true;
    const struct ly_ctx *ly_ctx;

    ly_ctx = sr_acquire_context(sr_session_get_connection(session));

    sk = nl_socket_alloc();
    if (sk == NULL) {
        log_error("Allocate nl socket failed");
        return;
    }

    status = nl_connect(sk, NETLINK_ROUTE);
    if (status != 0) {
        log_error("Connect to nl failed");
        nl_socket_free(sk);
        return;
    }

    status = rtnl_link_alloc_cache(sk, AF_UNSPEC, &cache);
    if (status != 0) {
        log_error("Alloc rtnl link cache failed");
        goto cleanup;
    }

    names = get_interface_names();
    SSET_FOR_EACH(name, names) {
        link = rtnl_link_get_by_name(cache, name);
        uint8_t oper_status = rtnl_link_get_operstate(link);
        switch (oper_status) {
            case IF_OPER_UP:
                oper_state = "up";
                break;
            case IF_OPER_DOWN:
                oper_state = "down";
                break;
            case IF_OPER_TESTING:
                oper_state = "testing";
                break;
            case IF_OPER_UNKNOWN:
                oper_state = "unknown";
                break;
            case IF_OPER_DORMANT:
                oper_state = "dormant";
                break;
            case IF_OPER_NOTPRESENT:
                oper_state = "not-present";
                break;
            case IF_OPER_LOWERLAYERDOWN:
                oper_state = "lower-layer-down";
                break;
            default:
                log_error("Invalid interface's operational state");
        }

        ds_clear(&path);
        ds_put_format(&path, "/ietf-interfaces:interfaces/interface[name='%s']/oper-status",
                      name);

        if (start) {
//            *parent = lyd_new_path(NULL, sr_get_context(sr_session_get_connection(session)),
//                                   ds_cstr(&path), oper_state, 0, 0);

            lyd_new_path(NULL, ly_ctx, ds_cstr(&path), oper_state, 0, parent);
            start = false;
        } else {
            lyd_new_path(*parent, NULL, ds_cstr(&path), oper_state, 0, NULL);
        }

        rtnl_link_put(link);
    }

    ds_destroy(&path);

cleanup:

    sr_release_context(sr_session_get_connection(session));

    if (NULL != cache) {
        nl_cache_free(cache);
    }

    if (sk != NULL) {
        nl_close(sk);
        nl_socket_free(sk);
    }
}

void update_ips(struct shash *interfaces, sr_session_ctx_t *session)
{
    struct shash ips;
    struct shash_node *node, *node_next;

    shash_init(&ips);
    collect_ips(interfaces, &ips);
    save_ips(&ips, session);

    SHASH_FOR_EACH_SAFE(node, node_next, &ips) {
        ip_destory((struct ip*)node->data);
    }

    shash_destroy(&ips);
}

void update_interfaces_speed(struct shash *interfaces, sr_session_ctx_t *session)
{
    struct ifaddrs *addrs, *addr;
    unsigned int speed = 0;
    uint64_t speed_bps = 0;
    int rc = SR_ERR_OK;
    struct ds path = DS_EMPTY_INITIALIZER;
    sr_val_t val = {0};
    sr_datastore_t ds;

    if (getifaddrs(&addrs) != 0) {
        log_error("Get interface addresses failed");
        return;
    }

    ds = sr_session_get_ds(session);
    sr_session_switch_ds(session, SR_DS_OPERATIONAL);

    addr = addrs;
    while (addr) {
        if (shash_find(interfaces, addr->ifa_name) == NULL) {
            addr = addr->ifa_next;
            continue;
        }

        get_interface_speed(addr->ifa_name, &speed);
        if (speed != -1) {
            speed_bps = (uint64_t)speed * 1000000ul;
        } else {
            speed_bps = -1;
        }

        ds_clear(&path);
        ds_put_format(&path,
                      "/ietf-interfaces:interfaces/interface[name='%s']/speed",
                      addr->ifa_name);
        val.xpath = ds_cstr(&path);
        val.type = SR_UINT64_T;
        val.data.uint64_val = speed_bps;
        rc = sr_set_item(session, ds_cstr(&path), &val, SR_EDIT_DEFAULT);
        if (rc != SR_ERR_OK) {
            log_error("Set %s=%s failed: %s", ds_cstr(&path), speed_bps, sr_strerror(rc));
        }

        rc = sr_apply_changes(session, 0);
        if (rc != SR_ERR_OK) {
            log_error("Set interface's speed apply failed: %s", sr_strerror(rc));
            sr_discard_changes(session);
        }

        addr = addr->ifa_next;
    }

    ds_destroy(&path);
    freeifaddrs(addrs);

    sr_session_switch_ds(session, ds);
}


struct sset *get_interface_names()
{
    if (interface_names != NULL) {
        return interface_names;
    }

    struct if_nameindex *if_ni, *idx_p;
    struct nl_sock *sk = NULL;
    struct nl_cache *cache = NULL;
    struct rtnl_link *link = NULL;
    int status;

    interface_names = malloc(sizeof(struct sset));
    sset_init(interface_names);

    sk = nl_socket_alloc();
    if (sk == NULL) {
        log_fatal("Allocate nl socket failed when get interfaces name");
        exit(-1);
    }

    status = nl_connect(sk, NETLINK_ROUTE);
    if (status != 0) {
        log_fatal("Connect to nl failed whe get interfaces name");
        nl_socket_free(sk);
        exit(-1);
    }

    status = rtnl_link_alloc_cache(sk, AF_UNSPEC, &cache);
    if (status != 0) {
        log_fatal("Alloc rtnl link cache failed when get interfaces name");
        goto cleanup;
    }

    if_ni = if_nameindex();
    for (idx_p = if_ni;
         idx_p != NULL && idx_p->if_index != 0 && idx_p->if_name != NULL;
         idx_p++)
    {
        if (strstr(idx_p->if_name, "eno") == NULL &&
            strstr(idx_p->if_name, "swp") == NULL)
        {
            continue;
        }

        link = rtnl_link_get_by_name(cache, idx_p->if_name);
        unsigned int arptype = rtnl_link_get_arptype(link);
        if (arptype != 772 && arptype != 280 && arptype != 776) {
            // arptype 772-loopback  280-can 776-site
            sset_add(interface_names, idx_p->if_name);
        }

        rtnl_link_put(link);
    }

cleanup:
    if (cache != NULL) {
        nl_cache_free(cache);
    }

    if (sk != NULL) {
        nl_close(sk);
        nl_socket_free(sk);
    }

    return interface_names;
}

void destroy_interface_names()
{
    if (interface_names != NULL) {
        sset_destroy(interface_names);
    }
}
