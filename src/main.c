#include <unistd.h>
#include <string.h>
#include "sysrepo.h"
#include "sysrepo/xpath.h"

#include "shash.h"
#include "log.h"

#include "dynamic-string.h"
#include "lldp.h"
#include "interface.h"
#include "bridge.h"
#include "hardware.h"

volatile int exit_application = 0;
struct shash interfaces;

static int provider_cb(sr_session_ctx_t *session, const char *module_name, const char *xpath,
                                 const char *request_xpath, uint32_t requrest_id, struct lyd_node **parent,
                                 void *private_ctx)
{
    log_info("Get request path: %s\n", xpath);
    if (strcmp(module_name, "ietf-interfaces") == 0) {
        if (strcmp(xpath, "/ietf-interfaces:interfaces/interface/statistics") == 0) {
            interface_statistics_provider(session, parent);
        } else if (strcmp(xpath, "/ietf-interfaces:interfaces/interface/oper-status") == 0) {
            interface_oper_status_provider(session, parent);
        }
    } else if (strcmp(module_name, "ieee802-dot1ab-lldp") == 0) {
        if (strcmp(xpath, "/ieee802-dot1ab-lldp:lldp/port") == 0) {
            lldp_port_provider(session, parent);
        }
    }

    return SR_ERR_OK;
}

static int data_provider(sr_session_ctx_t *session)
{
    sr_subscription_ctx_t *statistics_subscription = NULL;
    sr_subscription_ctx_t *oper_status_subscription = NULL;
    sr_subscription_ctx_t *lldp_subscription = NULL;
    int rc = SR_ERR_OK;

    rc = sr_oper_get_subscribe(session, "ietf-interfaces", "/ietf-interfaces:interfaces/interface/statistics",
                               provider_cb, NULL, SR_SUBSCR_DEFAULT, &statistics_subscription);

    rc = sr_oper_get_subscribe(session, "ietf-interfaces", "/ietf-interfaces:interfaces/interface/oper-status",
                               provider_cb, NULL, SR_SUBSCR_DEFAULT, &oper_status_subscription);

    rc = sr_oper_get_subscribe(session, "ieee802-dot1ab-lldp", "/ieee802-dot1ab-lldp:lldp/port",
                               provider_cb, NULL, SR_SUBSCR_DEFAULT, &lldp_subscription);

    while (!exit_application) {
        sleep(1);
        update_ips(&interfaces, session);
        update_interfaces_speed(&interfaces, session);
    }

cleanup:
    if (NULL != statistics_subscription) {
        sr_unsubscribe(statistics_subscription);
    }

    if (NULL != oper_status_subscription) {
        sr_unsubscribe(oper_status_subscription);
    }

    if (NULL != lldp_subscription) {
        sr_unsubscribe(lldp_subscription);
    }

    return rc;
}

int main(int argc, char *argv[])
{
    struct shash_node *node, *node_next;
    struct interface *intf;
    sr_conn_ctx_t *connection = NULL;
    sr_session_ctx_t *session = NULL;
    int rc = SR_ERR_OK;

    log_set_level(LOG_INFO);

    shash_init(&interfaces);

    // set sysrepo's log level
    sr_log_stderr(SR_LL_WRN);

    rc = sr_connect(SR_CONN_DEFAULT, &connection);
    if (rc != SR_ERR_OK) {
        log_fatal("Connection to sysrepo failed: %s\n", sr_strerror(rc));
        goto cleanup;
    }

    rc = sr_session_start(connection, SR_DS_RUNNING, &session);
    if (rc != SR_ERR_OK) {
        log_fatal("Get session from sysrepo's connection failed: %s\n", sr_strerror(rc));
        goto cleanup;
    }

    rc = sr_delete_item(session, "/ieee802-dot1ab-lldp:lldp", SR_EDIT_DEFAULT);
    if (rc != SR_ERR_OK) {
        log_error("Delete lldp from sysrepo failed: %s", sr_strerror(rc));
    }

    rc = sr_apply_changes(session, 0);
    if (rc != SR_ERR_OK) {
        log_error("Delete lldp from sysrepo apply failed: %s", sr_strerror(rc));
    }

    // delete all interfaces in sysrepo
    rc = sr_delete_item(session, "/ietf-interfaces:interfaces", SR_EDIT_DEFAULT);
    if (rc != SR_ERR_OK) {
        log_error("Delete interface from sysrepo failed: %s", sr_strerror(rc));
    }
    rc = sr_apply_changes(session, 0);
    if (rc != SR_ERR_OK) {
        log_error("Delete interface from sysrepo apply failed: %s", sr_strerror(rc));
    }

    // collect interfaces' info
    collect_interfaces(&interfaces);
    SHASH_FOR_EACH(node, &interfaces) {
        intf = (struct interface*)node->data;
        save_interface_running(intf, session);
    }


    sr_session_switch_ds(session, SR_DS_OPERATIONAL);
    SHASH_FOR_EACH(node, &interfaces) {
        intf = (struct interface*)node->data;
        save_interface_operational(intf, session);
    }
    sr_session_switch_ds(session, SR_DS_RUNNING);

    save_hardware_chassis(session);
    update_ips(&interfaces, session);

    struct shash bridges;
    struct shash_node *br_node = NULL;
    shash_init(&bridges);
    collect_bridges(&bridges);
    save_bridges(&bridges, session);
    shash_destroy_free_data(&bridges);

    rc = data_provider(session);

cleanup:
    sr_disconnect(connection);

    SHASH_FOR_EACH_SAFE(node, node_next, &interfaces) {
        interface_destroy((struct interface*)node->data);
    }
    shash_destroy(&interfaces);

    destroy_interface_names();

    return 0;
}

