#include "bridge.h"

#include <stdio.h>
#include <net/if.h>
#include <sys/socket.h>
#include <string.h>

#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/link/bridge.h>
#include <cjson/cJSON.h>

#include "log.h"
#include "dynamic-string.h"
#include "utils.h"

#define BUFFER_LENGTH 1024

void get_bridge_vlan(const char *br_name, br_vlan_t *vlan)
{
    FILE* fp = NULL;
    char buffer[BUFFER_LENGTH] = {0};
    struct ds command = DS_EMPTY_INITIALIZER;
    struct ds output = DS_EMPTY_INITIALIZER;
    const char *err = NULL;
    cJSON *root = NULL;
    const cJSON *bridge = NULL, *vlan_node = NULL, *id_node = NULL, *flags_node = NULL;
    const cJSON *flag_node = NULL;
    char *flag;
    int len = 0, id = 0;

    bzero(vlan, sizeof(*vlan));

    ds_put_format(&command, "bridge -j vlan show dev %s", br_name);

    fp = popen(ds_cstr(&command), "r");
    if (NULL == fp) {
        log_error("Execute command-%s failed", ds_cstr(&command));
        return;
    }

    while (fgets(buffer, BUFFER_LENGTH, fp) != NULL) {
        ds_put_cstr(&output, buffer);
    }

    ds_destroy(&command);
    pclose(fp);

    root = cJSON_Parse(ds_cstr(&output));
    if (NULL == root) {
        if ((err = cJSON_GetErrorPtr()) != NULL) {
            log_error("Parse bridge vlan's output failed: %s", err);
        }
    }

    bridge = cJSON_GetObjectItemCaseSensitive(root, br_name);
    if (NULL == bridge) {
        log_error("the output of bridge vlan show is invalid: %s", ds_cstr(&output));
        goto cleanup;
    }

    if (!cJSON_IsArray(bridge)) {
        log_warn("%s's vlan node is not array: %s", br_name, cJSON_Print(bridge));
        goto cleanup;
    }

    len = cJSON_GetArraySize(bridge);
    for (int i = 0; i < len; i++) {
        vlan_node = cJSON_GetArrayItem(bridge, i);
        id_node = cJSON_GetObjectItemCaseSensitive(vlan_node, "vlan");
        if (!cJSON_IsNumber(id_node)) {
            log_error("vlan id is inalid for %s", br_name);
            continue;
        }
        id = id_node->valueint;
        flags_node = cJSON_GetObjectItemCaseSensitive(vlan_node, "flags");

        if (NULL == flags_node) {
            for (int j = 0; j < VLAN_BITMAP_MAX; j++) {
                if (vlan->vlan_bitmap[j] == 0) {
                    vlan->vlan_bitmap[j] = id;
                    break;
                }
            }
        } else {
            if (!cJSON_IsArray(flags_node)) {
                log_error("The flags node of bridge is invalid: %s", cJSON_Print(flags_node));
                continue;
            }

            bool is_pvid = false, is_untagged = false;
            int n = cJSON_GetArraySize(flags_node);
            for (int j = 0; j < n; j++) {
                flag_node = cJSON_GetArrayItem(flags_node, j);
                if (cJSON_IsString(flag_node)) {
                    flag = cJSON_GetStringValue(flag_node);
                    if (strcmp(flag, "PVID") == 0) {
                        is_pvid = true;
                    } else if (strstr(flag, "Untagged") != NULL) {
                        is_untagged = true;
                    }
                }
            }

            if (is_pvid && vlan->pvid != 0) {
                log_error("There are multiple pvid");
            } else if (is_pvid) {
                vlan->pvid = id;
            } else if (is_untagged) {
                for (int j = 0; j < VLAN_BITMAP_MAX; j++) {
                    if (vlan->untagged_bitmap[j] == 0) {
                        vlan->untagged_bitmap[j] = id;
                        break;
                    }
                }
            }
        }
    }

cleanup:
    cJSON_Delete(root);
    ds_clear(&output);
}

void collect_bridges(struct shash *bridges)
{
    struct if_nameindex *if_ni, *idx_p;
    struct nl_sock *sk = NULL;
    struct nl_cache *cache = NULL;
    struct rtnl_link *link = NULL;
    bridge_t *br;
    int status = 0;
    br_vlan_t *vlan = NULL;
    char name[NAME_LEN] = { 0 };

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
        log_error("Allocate rtnl link cache failed");
        goto cleanup;
    }

    for (idx_p = if_ni;
         idx_p != NULL && idx_p->if_index != 0 && idx_p->if_name != NULL;
         idx_p++)
    {
        link = rtnl_link_get_by_name(cache, idx_p->if_name);
        if (link != NULL) {
            if (rtnl_link_is_bridge(link)) {
                br = (bridge_t *) malloc(sizeof(bridge_t));
                bzero(br, sizeof(bridge_t));
                strncpy(br->name, idx_p->if_name, sizeof(br->name));
                br->index = idx_p->if_index;

                struct nl_addr *addr = rtnl_link_get_addr(link);
                nl_addr2str(addr, br->hw_addr, sizeof(br->hw_addr) - 1);

                get_bridge_vlan(br->name, &br->vlan);

                shash_add(bridges, idx_p->if_name, br);
            }
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

void save_bridges(struct shash *bridges, sr_session_ctx_t *session)
{
    int rc = SR_ERR_OK;
    struct shash_node *br_node = NULL;
    bridge_t *br = NULL;
    struct ds path = DS_EMPTY_INITIALIZER;
    sr_val_t val = {0};

    rc = sr_delete_item(session,
                        "/ieee802-dot1q-bridge:bridges/bridge",
                        SR_EDIT_DEFAULT);
    if (rc != SR_ERR_OK) {
        log_error("Delete bridges from sysrepo failed: %s", sr_strerror(rc));
    }

    rc = sr_apply_changes(session, 0, 0);
    if (rc != SR_ERR_OK) {
        log_error("Delete bridge apply failed: %s", sr_strerror(rc));
        sr_discard_changes(session);
    }

    SHASH_FOR_EACH(br_node, bridges) {
        br = (bridge_t*)br_node->data;

        ds_clear(&path);
        ds_put_format(&path,
                      "/ieee802-dot1q-bridge:bridges/bridge[name='%s']/address",
                      br->name);
        val.type = SR_STRING_T;
        val.data.string_val = to_ieee_mac_addr(br->hw_addr);
        rc = sr_set_item(session, ds_cstr(&path), &val, 0);
        if (rc != SR_ERR_OK) {
            log_error("Set %s=%s failed: %s",
                      ds_cstr(&path),
                      val.data.string_val,
                      sr_strerror(rc));
        }

        ds_clear(&path);
        ds_put_format(&path,
                      "/ieee802-dot1q-bridge:bridges/bridge[name='%s']/bridge-type",
                      br->name);
        val.type = SR_IDENTITYREF_T;
        val.data.identityref_val = "provider-edge-bridge";
        rc = sr_set_item(session, ds_cstr(&path), &val, 0);
        if (rc != SR_ERR_OK) {
            log_error("Set %s=%s failed: %s",
                      ds_cstr(&path),
                      val.data.identityref_val,
                      sr_strerror(rc));
        }

        for (int i = 0; i < VLAN_BITMAP_MAX; i++) {
            if (br->vlan.vlan_bitmap[i] != 0) {
                char vlan_name[16] = {0};
                snprintf(vlan_name, 16, "vlan%d", br->vlan.vlan_bitmap[i]);

                ds_clear(&path);
                ds_put_format(&path,
                              "/ieee802-dot1q-bridge:bridges/bridge[name='%s']/component[name='%s']/type",
                              br->name,
                              vlan_name);
                val.type = SR_IDENTITYREF_T;
                val.data.identityref_val = "edge-relay-component";
                rc = sr_set_item(session, ds_cstr(&path), &val, 0);
                if (rc != SR_ERR_OK) {
                    log_error("Set %s=%s failed: %s",
                              ds_cstr(&path),
                              val.data.identityref_val,
                              sr_strerror(rc));
                }

                ds_clear(&path);
                ds_put_format(&path,
                              "/ieee802-dot1q-bridge:bridges/bridge[name='%s']/component[name='%s']/bridge-vlan/vlan[vid='%d']/name",
                              br->name,
                              vlan_name,
                              br->vlan.vlan_bitmap[i]);
                val.type = SR_STRING_T;
                val.data.string_val = vlan_name;
                rc = sr_set_item(session, ds_cstr(&path), &val, 0);
                if (rc != SR_ERR_OK) {
                    log_error("Set %s=%s failed: %s",
                              ds_cstr(&path),
                              vlan_name,
                              sr_strerror(rc));
                }
            } else {
                break;
            }
        }

        rc = sr_apply_changes(session, 0, 0);
        if (rc != SR_ERR_OK) {
            log_error("Set bridge's address and type apply failed: %s", sr_strerror(rc));
            sr_discard_changes(session);
        }
    }

    ds_destroy(&path);
}