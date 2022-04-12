#include "lldp.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "log.h"
#include "dynamic-string.h"
#include "utils.h"
#include "shash.h"

#define BUFFER_LENGTH 1024


struct capability *capability_init()
{
    struct capability *capability = (struct capability*)calloc(1, sizeof(struct capability));
    return capability;
}

struct capability *get_capability_from_node(xmlNodePtr node)
{
    struct capability *capability = NULL;
    xmlChar *value = NULL;

    if (node == NULL || !xmlStrEqual(node->name, BAD_CAST "capability")) {
        return NULL;
    }

    capability = capability_init();
    capability->type = get_node_prop_str(node, "type");
    get_node_prop_enabled(node, "enabled", &capability->enabled);

    return capability;
}

void capability_destroy(struct capability *capability)
{
    free(capability);
}

inline struct chassis *chassis_init()
{
    struct chassis *chassis = (struct chassis*)calloc(1, sizeof(struct chassis));
    svec_init(&chassis->mgmt_ip);
    list_init(&chassis->capabilities);
    return chassis;
}

struct chassis *get_chassis_from_node(xmlNodePtr node)
{
    struct chassis *chassis = NULL;
    xmlNodePtr current;
    xmlChar *value = NULL;
    const xmlChar *name = NULL;

    if (node == NULL || !xmlStrEqual(node->name, BAD_CAST "chassis")) {
        return NULL;
    }

    chassis = chassis_init();

    current = node->children;
    while (current != NULL) {
        name = current->name;
        if (xmlStrEqual(name, BAD_CAST "id")) {
            chassis->id_type = get_node_prop_str(current, "type");
            chassis->id = get_node_content_str(current);
        } else if (xmlStrEqual(name, BAD_CAST "name")) {
            chassis->name = get_node_content_str(current);
        } else if (xmlStrEqual(name, BAD_CAST "descr")) {
            chassis->description = get_node_content_str(current);
        } else if (xmlStrEqual(name, BAD_CAST "ttl")) {
            value = xmlNodeGetContent(current);
            chassis->ttl = atoi(value);
            xmlFree(value);
        } else if (xmlStrEqual(name, BAD_CAST "mgmt-ip")) {
            char *ip = get_node_content_str(current);
            svec_add_nocopy(&chassis->mgmt_ip, ip);
        } else if (xmlStrEqual(name, BAD_CAST "capability")) {
            struct capability *capability = get_capability_from_node(current);
            list_push_back(&chassis->capabilities, &capability->node);
        }

        current = current->next;
    }

    return chassis;
}

void chassis_destroy(struct chassis* chassis)
{
    struct capability *capa, *capa_next;
    if (!chassis) {
        return;
    }

    if (chassis->id) {
        free(chassis->id);
    }

    if (chassis->id_type) {
        free(chassis->id_type);
    }

    if (chassis->name) {
        free(chassis->name);
    }

    if (chassis->description) {
        free(chassis->description);
    }

    svec_destroy(&chassis->mgmt_ip);
    LIST_FOR_EACH_SAFE(capa, capa_next, node, &chassis->capabilities) {
        list_remove(&capa->node);
        capability_destroy(capa);
    }

    free(chassis);
}

inline struct advertised *advertised_init()
{
    struct advertised *ad =(struct advertised*)calloc(1, sizeof(struct advertised));
    return ad;
}

struct advertised *get_advertised_from_node(xmlNodePtr node)
{
    struct advertised *ad = NULL;
    xmlChar *value = NULL;

    if (node == NULL || !xmlStrEqual(node->name, BAD_CAST "advertised")) {
        return NULL;
    }

    ad = advertised_init();
    ad->type = get_node_prop_str(node, "type");
    get_node_prop_bool(node, "hd", &ad->hd);
    get_node_prop_bool(node, "fd", &ad->fd);

    return ad;
}

void advertised_destroy(struct advertised *ad)
{
    if (!ad) {
        return;
    }

    if (ad->type) {
        free(ad->type);
    }

    free(ad);
}

struct negotiation *negotiation_init()
{
    struct negotiation *negotiation =
            (struct negotiation*)calloc(1, sizeof(struct negotiation));
    list_init(&negotiation->advertise);
    return negotiation;
}

struct negotiation *get_negotiation_from_node(xmlNodePtr node)
{
    struct negotiation *negotiation = NULL;
    xmlNodePtr current;
    xmlChar *value = NULL;

    if (node == NULL || !xmlStrEqual(node->name, BAD_CAST "auto-negotiation")) {
        return NULL;
    }

    negotiation = negotiation_init();
    get_node_prop_bool(node, "supported", &negotiation->supported);
    get_node_prop_bool(node, "enabled", &negotiation->enabled);

    current = node->children;
    while (current != NULL) {
        if (xmlStrEqual(current->name, BAD_CAST "advertised")) {
            struct advertised *ad = get_advertised_from_node(current);
            list_push_back(&negotiation->advertise, &ad->node);
        } else if (xmlStrEqual(current->name, BAD_CAST "current")) {
            negotiation->current = get_node_content_str(current);
        }

        current = current->next;
    }

    return negotiation;
}

void negotiation_destroy(struct negotiation* negotiation)
{
    struct advertised *ad, *ad_next;

    if (!negotiation) {
        return;
    }

    LIST_FOR_EACH_SAFE(ad, ad_next, node, &negotiation->advertise) {
        list_remove(&ad->node);
        advertised_destroy(ad);
    }

    if (negotiation->current) {
        free(negotiation->current);
    }

    free(negotiation);
}

inline struct port *port_init()
{
    struct port *port = (struct port*)calloc(1, sizeof(struct port));
    return port;
}

struct port *get_port_from_node(xmlNodePtr node)
{
    struct port *port = NULL;
    xmlNodePtr current;
    const xmlChar *name = NULL;
    xmlChar *value = NULL;

    if (node == NULL || !xmlStrEqual(node->name,BAD_CAST "port")) {
        return NULL;
    }

    port = port_init();

    current = node->children;
    while (current != NULL) {
        name = current->name;
        if (xmlStrEqual(name, BAD_CAST "id")) {
            port->id_type = get_node_prop_str(current, "type");
            port->id = get_node_content_str(current);
        } else if (xmlStrEqual(name, BAD_CAST "descr")) {
            port->description = get_node_content_str(current);
        } else if (xmlStrEqual(name, BAD_CAST "ttl")) {
            value = xmlNodeGetContent(current);
            port->ttl = atoi(value);
            xmlFree(value);
        } else if (xmlStrEqual(name, BAD_CAST "mfs")) {
            value = xmlNodeGetContent(current);
            port->mfs = atoi(value);
            xmlFree(value);
        } else if (xmlStrEqual(name, BAD_CAST "auto-negotiation")) {
            port->negotiation = get_negotiation_from_node(current);
        }

        current = current->next;
    }

    return port;
}

void port_destroy(struct port *port)
{
    if (!port) {
        return;
    }

    if (port->id) {
        free(port->id);
    }

    if (port->id_type) {
        free(port->id_type);
    }

    if (port->description) {
        free(port->description);
    }

    if (port->negotiation) {
        negotiation_destroy(port->negotiation);
    }

    free(port);
}

inline struct vlan *vlan_init()
{
    struct vlan *vlan = (struct vlan*)calloc(1, sizeof(struct vlan));
    return vlan;
}

struct vlan *get_vlan_from_node(xmlNodePtr node)
{
    struct vlan *vlan = NULL;
    xmlChar *value = NULL;

    if (node == NULL || !xmlStrEqual(node->name, BAD_CAST "vlan")) {
        return NULL;
    }

    vlan = vlan_init();
    vlan->id = get_node_prop_str(node, "vlan-id");
    get_node_prop_bool(node, "pvid", &vlan->pvid);
    vlan->value = get_node_content_str(node);

    return vlan;
}

void vlan_destroy(struct vlan *vlan)
{
    if (!vlan) {
        return;
    }

    if (vlan->id) {
        free(vlan->id);
    }

    if (vlan->value) {
        free(vlan->value);
    }

    free(vlan);
}

struct ppvid *ppvid_init()
{
    struct ppvid *ppvid = (struct ppvid*)calloc(1, sizeof(struct ppvid));
    return ppvid;
}

struct ppvid *get_ppvid_from_node(xmlNodePtr node)
{
    struct ppvid *ppvid = NULL;
    xmlChar *value = NULL;

    if (node == NULL || !xmlStrEqual(node->name, BAD_CAST "ppvid")) {
        return NULL;
    }

    ppvid = ppvid_init();

    get_node_prop_bool(node, "supported", &ppvid->supported);
    get_node_prop_bool(node, "enabled", &ppvid->enabled);

    return ppvid;
}

void ppvid_destroy(struct ppvid *ppvid)
{
    if (!ppvid) {
        return;
    }

    free(ppvid);
}

lldp_t *lldp_init()
{
    lldp_t *lldp = (lldp_t*)calloc(1, sizeof(lldp_t));
    svec_init(&lldp->pi);
    return lldp;
}

lldp_t *get_lldp_from_node(xmlNodePtr node)
{
    lldp_t *lldp = NULL;
    xmlNodePtr current;
    const xmlChar *name;
    xmlChar *value;

    if (node == NULL || !xmlStrEqual(node->name, BAD_CAST "interface")) {
        return NULL;
    }

    lldp = lldp_init();

    current = node->children;
    while (current != NULL) {
        name = current->name;
        if (xmlStrEqual(name, "pi")) {
            char *pi = get_node_content_str(current);
            svec_add_nocopy(&lldp->pi, pi);
        } else if (xmlStrEqual(name, BAD_CAST "vlan")) {
            lldp->vlan = get_vlan_from_node(current);
        } else if (xmlStrEqual(name, BAD_CAST "ppvid")) {
            lldp->ppvid = get_ppvid_from_node(current);
        } else if (xmlStrEqual(name, BAD_CAST "chassis")) {
            lldp->chassis = get_chassis_from_node(current);
        } else if (xmlStrEqual(name, BAD_CAST "port")) {
            lldp->port = get_port_from_node(current);
        }

        current = current->next;
    }

    return lldp;
}

void lldp_destroy(lldp_t *lldp)
{
    if (!lldp) {
        return;
    }
    if (lldp->name != NULL) {
        free(lldp->name);
    }

    if (lldp->via) {
        free(lldp->via);
    }

    if (lldp->rid) {
        free(lldp->rid);
    }

    if (lldp->chassis) {
        chassis_destroy(lldp->chassis);
    }

    if (lldp->port) {
        port_destroy(lldp->port);
    }

    if (lldp->vlan) {
        vlan_destroy(lldp->vlan);
    }

    if (lldp->ppvid) {
        ppvid_destroy(lldp->ppvid);
    }

    svec_destroy(&lldp->pi);

    free(lldp);
}

char *get_node_content_str(xmlNodePtr node)
{
    char *value = NULL;
    xmlChar *content =NULL;

    if (node == NULL) {
        return NULL;
    }

    content = xmlNodeGetContent(node);
    value = strdup(content);
    xmlFree(content);
    return value;
}

char *get_node_prop_str(xmlNodePtr node, const char *attrib_name)
{
    char *value = NULL;
    xmlChar *attrib_value = NULL;

    if (node == NULL || !xmlHasProp(node, BAD_CAST attrib_name)) {
        return NULL;
    }

    attrib_value = xmlGetProp(node, BAD_CAST attrib_name);
    value = strdup(attrib_value);
    xmlFree(attrib_value);

    return value;
}

void get_node_prop_bool(xmlNodePtr node, const char *attrib_name, bool *value)
{
    xmlChar *attrib_value = NULL;

    if (node == NULL || !xmlHasProp(node, BAD_CAST attrib_name)) {
        return;
    }

    attrib_value = xmlGetProp(node, BAD_CAST attrib_name);
    *value = xmlStrEqual(attrib_value, BAD_CAST "true") ? true : false;
    xmlFree(attrib_value);
}

void get_node_prop_enabled(xmlNodePtr node, const char *attrib_name, bool *value)
{
    xmlChar *attrib_value = NULL;

    if (node == NULL || !xmlHasProp(node, BAD_CAST attrib_name)) {
        return;
    }

    attrib_value = xmlGetProp(node, BAD_CAST attrib_name);
    *value = xmlStrEqual(attrib_value, BAD_CAST "on") ? true : false;
    xmlFree(attrib_value);
}

static inline void swp_str(char **s1, char **s2)
{
    char *tmp;
    tmp = *s1;
    *s1 = *s2;
    *s2 = tmp;
}

void lldp_node_destroy(lldp_node_t *lldp_node)
{
    if (lldp_node == NULL) {
        return;
    }

    if (lldp_node->name)
        free(lldp_node->name);

    if (lldp_node->rid)
        free(lldp_node->rid);

    if (lldp_node->chassis_id)
        free(lldp_node->chassis_id);

    if (lldp_node->chassis_type)
        free(lldp_node->chassis_type);

    if (lldp_node->chassis_name)
        free(lldp_node->chassis_name);

    if (lldp_node->chassis_description)
        free(lldp_node->chassis_description);

    if (lldp_node->port_id)
        free(lldp_node->port_id);

    if (lldp_node->port_type)
        free(lldp_node->port_type);

    if (lldp_node->port_description)
        free(lldp_node->port_description);

    free(lldp_node);
}

static inline void add_new_string_path(
        const char *format, const char *name, const char *port_mac, uint64_t age,
        int index, char *node_name, struct lyd_node **parent, char *value)
{
    struct ds path = DS_EMPTY_INITIALIZER;

    ds_put_format(&path, format, name, port_mac, age, index, node_name);
    lyd_new_path(*parent, NULL, ds_cstr(&path), value, 0, 0);
    ds_destroy(&path);
}

void lldp_port_provider(sr_session_ctx_t *session, struct lyd_node **parent)
{
    FILE* fp = NULL;
    char buffer[BUFFER_LENGTH] = {0};
    struct ds s = DS_EMPTY_INITIALIZER;
    struct ds path = DS_EMPTY_INITIALIZER;
    xmlDocPtr doc;
    xmlNodePtr current;
    const char *format = "/ieee802-dot1ab-lldp:lldp/port[name='%s'][dest-mac-address='%s']/remote-systems-data[time-mark='%" PRIu64 "'][remote-index='%d']/%s";
    bool start = true;
    const struct ly_ctx *ly_ctx;

    ly_ctx = sr_acquire_context(sr_session_get_connection(session));

    fp = popen("lldpcli show neighbors -f xml" ,"r");
    if (fp == NULL) {
        log_error("Execute lldpctl command failed");
        return;
    }

    while (fgets(buffer, BUFFER_LENGTH, fp) != NULL) {
        ds_put_cstr(&s, buffer);
    }

    pclose(fp);

    doc = xmlParseDoc((xmlChar*)ds_cstr(&s));
    if (doc == NULL) {
        log_error("Parse lldp's xml failed");
        goto end;
    }

    current = xmlDocGetRootElement(doc);
    if (current == NULL) {
        log_warn("The root of lldp's xml is empty");
        goto end;
    }

    if (xmlStrcmp(current->name, BAD_CAST "lldp")) {
        log_warn("There is no lldp node in lldp's xml");
        goto end;
    }

    current = current->children;

    while (current != NULL) {
        if (xmlStrEqual(current->name, BAD_CAST "interface")) {
            lldp_t *lldp = get_lldp_from_node(current);

            lldp->name = get_node_prop_str(current, "name");
            lldp->via = get_node_prop_str(current, "via");
            lldp->rid = get_node_prop_str(current, "rid");
            char *age = get_node_prop_str(current, "age");
            lldp->age = get_age(age);
            free(age);

            to_ieee_mac_addr(lldp->port->id);
            int index = atoi(lldp->rid);
            ds_clear(&path);
            ds_put_format(&path, format, lldp->name, lldp->port->id, lldp->age, index, "system-name");
            if (start) {
//                *parent = lyd_new_path(NULL, sr_get_context(sr_session_get_connection(session)),
//                                       ds_cstr(&path), lldp->chassis->name, 0, 0);
		lyd_new_path(NULL, ly_ctx, ds_cstr(&path), lldp->chassis->name, 0, parent);
                start = false;
            } else {
                lyd_new_path(*parent, NULL, ds_cstr(&path), lldp->chassis->name, 0, NULL);
            }

            add_new_string_path(format, lldp->name, lldp->port->id, lldp->age, index,
                                "system-description", parent, lldp->chassis->description);

            // TODO(sgk):
            add_new_string_path(format, lldp->name, lldp->port->id, lldp->age, index,
                                "chassis-id-subtype", parent, "mac-address");
            add_new_string_path(format, lldp->name, lldp->port->id, lldp->age, index,
                                "chassis-id", parent, lldp->chassis->id);

            // TODO(sgk):
            add_new_string_path(format, lldp->name, lldp->port->id, lldp->age, index,
                                "port-id-subtype", parent, "mac-address");
            add_new_string_path(format, lldp->name, lldp->port->id, lldp->age, index,
                                "port-id", parent, lldp->port->id + 9);

            add_new_string_path(format, lldp->name, lldp->port->id, lldp->age, index,
                                "port-desc", parent, lldp->port->description);


            lldp_destroy(lldp);
        }

        current = current->next;
    }

end:
    sr_release_context(sr_session_get_connection(session));

    if (doc) {
        xmlFreeDoc(doc);
    }
    ds_destroy(&s);
    ds_destroy(&path);
}
