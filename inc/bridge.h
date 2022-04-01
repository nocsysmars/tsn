#include "shash.h"

#include <stdint.h>
#include <sysrepo.h>

#define VLAN_BITMAP_MAX 4096
#define NAME_LEN 32

typedef struct br_vlan_s {
    uint16_t pvid;
    uint32_t vlan_bitmap[VLAN_BITMAP_MAX];
    uint32_t untagged_bitmap[VLAN_BITMAP_MAX];
} br_vlan_t;

typedef struct bridge_s {
    char name[NAME_LEN];
    unsigned int index;
    char hw_addr[24];
    char types[32];
    br_vlan_t vlan;
} bridge_t;

void get_bridge_vlan(const char *br_name, br_vlan_t *vlan);
void collect_bridges(struct shash *bridges);

void save_bridges(struct shash *bridges, sr_session_ctx_t *session);

