#ifndef REPO_H
#define REPO_H

#include <sysrepo.h>
#include <pthread.h>

#include "list.h"

typedef enum {
    DELETE_ITEM = 0,
    SET_ITEM,
} action_type_t;

typedef struct action_s {
    struct list_node node;
    action_type_t type;
    char *path;
    sr_val_t val;
} action_t;

typedef struct oper_actions_s {
    pthread_mutex_t mutex;
    struct list_node actions;
} oper_actions_t;

void oper_actions_init(oper_actions_t *actions);

void clear_sysrepo();

#endif /* repo.h */