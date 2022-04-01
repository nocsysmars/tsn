#include "repo.h"

#include <sysrepo.h>

void oper_actions_init(oper_actions_t *oper_actions)
{
    pthread_mutex_init(&oper_actions->mutex, NULL);
    list_init(&oper_actions->actions);
}

void clear_sysrepo(sr_session_ctx_t *session)
{
    int rc = SR_ERR_OK;

    //rc = sr_delete_item(session, "/ieee802-dot1ab-lldp:lldp/port", SR_EDIT_DEFAULT);
    //if (rc != SR_ERR_OK) {
    //    fprintf(stderr, "Delete all lldp from sysrepo failed: %s\n", sr_strerror(rc));
    //}

    //rc = sr_apply_changes(session, 0, 0);
    //if (rc != SR_ERR_OK) {
    //    fprintf(stderr, "Apply delete lldp failed: %s\n", sr_strerror(rc));
    //}

    //printf("Delete all lldp success\n");

    rc = sr_delete_item(session, "/ietf-interfaces:interfaces", SR_EDIT_DEFAULT);
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Delete interfaces from sysrepo failed: %s\n", sr_strerror(rc));
    }

    rc = sr_apply_changes(session, 0, 0);
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Apply delete interfaces failed: %s\n", sr_strerror(rc));
    }
}