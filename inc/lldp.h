#ifndef LLDP_H
#define LLDP_H 1

#include <stdbool.h>
#include <stdint.h>
#include <sysrepo.h>

#include "list.h"
#include "svec.h"

#include "libxml/parser.h"

struct capability {
    struct list_node node;
    char *type;
    bool enabled;
};

struct chassis {
    char *id;
    char *id_type;
    char *name;
    char *description;
    uint32_t ttl;
    struct svec mgmt_ip;
    struct list_node capabilities;
};

struct advertised {
    struct list_node node;
    char* type;
    bool hd;
    bool fd;
};

struct negotiation {
    struct list_node advertise;
    bool supported;
    bool enabled;
    char *current;
};

struct port {
    char *id;
    char *id_type;
    char *description;
    uint32_t ttl;
    uint32_t mfs;
    struct negotiation* negotiation;
};

struct vlan {
    char *id;
    bool pvid;
    char *value;
};

struct ppvid {
    bool supported;
    bool enabled;
};

typedef struct lldp_s {
    struct list_node node;
    char *name;
    char *via;
    char *rid;
    uint64_t age;
    struct chassis *chassis;
    struct port *port;
    struct vlan *vlan;
    struct ppvid *ppvid;
    struct svec pi;
} lldp_t;

typedef struct lldp_node_s {
    char *name;
    char *rid;
    uint64_t age;
    char *chassis_id;
    char *chassis_type;
    char *chassis_name;
    char *chassis_description;
    char *port_id;
    char *port_type;
    char *port_description;
} lldp_node_t;

struct capability *capability_init();
struct capability *get_capability_from_node(xmlNodePtr node);
void capability_destroy(struct capability *capability);

struct chassis *chassis_init();
struct chassis *get_chassis_from_node(xmlNodePtr node);
void chassis_destroy(struct chassis *);

struct advertised *advertised_init();
struct advertised *get_advertised_from_node(xmlNodePtr node);
void advertised_destroy(struct advertised *);

struct negotiation *negotiation_init();
struct negotiation *get_negotiation_from_node(xmlNodePtr node);
void negotiation_destroy(struct negotiation *);

struct port *port_init();
struct port *get_port_from_node(xmlNodePtr node);
void port_destroy(struct port *);

struct vlan *vlan_init();
struct vlan *get_vlan_from_node(xmlNodePtr node);
void vlan_destroy(struct vlan *);

struct ppvid *ppvid_init();
struct ppvid *get_ppvid_from_node(xmlNodePtr node);
void ppvid_destroy(struct ppvid *);

lldp_t *lldp_init();
lldp_t *get_lldp_from_node(xmlNodePtr node);
void lldp_destroy(lldp_t *);

char *get_node_content_str(xmlNodePtr node);
char *get_node_prop_str(xmlNodePtr node, const char *attrib_name);
void get_node_prop_bool(xmlNodePtr node, const char *attrib_name, bool *value);
void get_node_prop_enabled(xmlNodePtr node, const char *attrib_name, bool *value);

void lldp_port_provider(sr_session_ctx_t *session, struct lyd_node **parent);

#endif /* LLDP_H */