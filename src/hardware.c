#include "hardware.h"

#include <ctype.h>
#include <fcntl.h>
#include <libxml/parser.h>
#include <string.h>
#include <unistd.h>

#include "dynamic-string.h"
#include "log.h"

#define BUFFER_LEN 64

#define PRODUCT_NAME_PATH "/sys/devices/virtual/dmi/id/product_name"
#define PRODUCT_VERSION_PATH "/sys/devices/virtual/dmi/id/product_version"
#define SYS_MODEL_PATH "/sys/firmware/devicetree/base/model"

static void trimLineCRLF(char *line)
{
    char *c = line;
    while (*c != '\0') {
        if (*c == '\r' || *c == '\n') {
            *c = '\0';
            return;
        }
        c++;
    }
}

static char *getHardwareVersion()
{
    struct ds res = DS_EMPTY_INITIALIZER;
    char buffer[BUFFER_LEN] = {0};
    char *hwVersion = NULL;

    if (access(PRODUCT_NAME_PATH, R_OK) != -1 && access(PRODUCT_VERSION_PATH, R_OK) != -1) {
        int fd_name = open(PRODUCT_NAME_PATH, O_RDONLY);
        int fd_version = open(PRODUCT_VERSION_PATH, O_RDONLY);
        if (fd_name != -1 && fd_version != -1) {
            memset(buffer, 0, sizeof(buffer));
            while(read(fd_name, buffer, sizeof(buffer) - 1) > 0) {
                trimLineCRLF(buffer);
                ds_put_cstr(&res, buffer);
                memset(buffer, 0, sizeof(buffer));
            }
            ds_put_char(&res, ' ');
            memset(buffer, 0, sizeof(buffer));
            while(read(fd_version, buffer, sizeof(buffer) - 1) > 0) {
                trimLineCRLF(buffer);
                ds_put_cstr(&res, buffer);
                memset(buffer, 0, sizeof(buffer));
            }

            hwVersion = ds_steal_cstr(&res);
        }

        close(fd_name);
        close(fd_version);
        if (NULL != hwVersion) {
            ds_destroy(&res);
            return hwVersion;
        }
    }

    if (access(SYS_MODEL_PATH, R_OK) != -1) {
        int fd = open(SYS_MODEL_PATH, O_RDONLY);
        if (fd != -1) {
            memset(buffer, 0, sizeof(buffer));
            while (read(fd, buffer, sizeof(buffer) - 1) > 0) {
                trimLineCRLF(buffer);
                ds_put_cstr(&res, buffer);
                memset(buffer, 0, sizeof(buffer));
            }

            hwVersion = ds_steal_cstr(&res);
        }

        close(fd);
        if (NULL != hwVersion) {
            ds_destroy(&res);
            return hwVersion;
        }
    }

    FILE *fp = popen("uname -m", "r");
    if (NULL == fp) {
        log_error("Get hardware version failed");
        return NULL;
    }

    while (fgets(buffer, BUFFER_LEN, fp) != NULL) {
        ds_put_cstr(&res, buffer);
    }

    pclose(fp);

    hwVersion = ds_steal_cstr(&res);
    trimLineCRLF(hwVersion);
    ds_destroy(&res);
    return hwVersion;
}

static char *getSoftwareVersion()
{
    FILE *fp = NULL;
    struct ds res = DS_EMPTY_INITIALIZER;
    char buffer[BUFFER_LEN] = {0};
    char *temp = NULL, *swVersion = NULL, *c = NULL;

    fp = popen("lsb_release -d", "r");
    if (NULL == fp) {
        log_error("Execute lsb_release -d failed");
        return NULL;
    }

    while (fgets(buffer, BUFFER_LEN, fp) != NULL) {
        ds_put_cstr(&res, buffer);
    }

    pclose(fp);

    temp = ds_cstr(&res);
    if (strstr(temp, "Description:") == NULL) {
        log_error("Get software version failed: Invalid lsb release");
        ds_destroy(&res);
        return NULL;
    }

    temp += 12;
    while(*temp != '\0') {
        if (isspace(*temp)) {
            temp++;
        } else {
            break;
        }
    }

    trimLineCRLF(temp);
    swVersion = strdup(temp);

    ds_destroy(&res);
    return swVersion;
}

static char *getChassisId()
{
    FILE *fp = NULL;
    char buffer[BUFFER_LEN] = {0};
    struct ds res = DS_EMPTY_INITIALIZER;
    xmlDocPtr doc;
    xmlNodePtr root, current, child;
    char *chassis_id = NULL;

    fp = popen("lldpcli -f xml show chassis summary", "r");
    if (NULL == fp) {
        log_error("Execute lldpcli -f xml show chassis summary failed");
        return NULL;
    }

    while (fgets(buffer, BUFFER_LEN, fp) != NULL) {
        ds_put_cstr(&res, buffer);
    }

    pclose(fp);

    doc = xmlParseDoc((xmlChar*)ds_cstr(&res));
    if (NULL == doc) {
        log_error("Parse lldp local chassis failed");
        goto end;
    }

    root = xmlDocGetRootElement(doc);
    if (NULL == root) {
        log_error("Invalid xml");
        goto end;
    }

    if (!xmlStrEqual(root->name, BAD_CAST "local-chassis")) {
        log_error("There is no local-chassis node");
        goto end;
    }

    current = root->children;
    while (current != NULL) {
        if (xmlStrEqual(current->name, BAD_CAST "chassis")) {
            child = current->children;
            while (child != NULL) {
                if (xmlStrEqual(child->name, BAD_CAST "id")) {
                    if (xmlHasProp(child, BAD_CAST "type") &&
                        xmlStrEqual(xmlGetProp(child, BAD_CAST "type"), "mac"))
                    {
                        chassis_id = strdup(xmlNodeGetContent(child));
                        goto end;
                    }
                }
                child = child->next;
            }
        }
        current = current->next;
    }

end:
    if (doc) {
        xmlFreeDoc(doc);
    }
    ds_destroy(&res);

    return chassis_id;
}

void save_hardware_chassis(sr_session_ctx_t *session)
{
    struct ds path = DS_EMPTY_INITIALIZER;
    char *hwVersion = NULL, *swVersion = NULL, *chassis_id = NULL;
    const char *format = "/ietf-hardware:hardware/component[name='%s']/%s";
    sr_val_t val = {0};
    int rc = SR_ERR_OK;

    hwVersion = getHardwareVersion();
    swVersion = getSoftwareVersion();
    chassis_id = getChassisId();

    if (NULL == chassis_id) {
        log_error("Get chassis failed");
        return;
    }

    ds_put_format(&path, format, chassis_id, "class");
    val.type = SR_IDENTITYREF_T;
    val.data.identityref_val = "iana-hardware:chassis";
    rc = sr_set_item(session, ds_cstr(&path), &val, 0);
    if (rc != SR_ERR_OK) {
        log_error("Set %s=%s failed: %s", ds_cstr(&path),
                  val.data.identityref_val,
                  sr_strerror(rc));
        goto cleanup;
    }

    rc = sr_apply_changes(session, 0, 0);
    if (rc != SR_ERR_OK) {
        log_error("Apply failed when set chassis's class: %s", sr_strerror(rc));
        sr_discard_changes(session);
        goto cleanup;
    }

    sr_session_switch_ds(session, SR_DS_OPERATIONAL);

    ds_clear(&path);
    ds_put_format(&path, format, chassis_id, "hardware-rev");
    val.type = SR_STRING_T;
    val.data.string_val = hwVersion;
    rc = sr_set_item(session, ds_cstr(&path), &val, 0);
    if (rc != SR_ERR_OK) {
        log_error("Set %s=%s failed: %s", ds_cstr(&path), val.data.string_val,
                  sr_strerror(rc));
    }

    ds_clear(&path);
    ds_put_format(&path, format, chassis_id, "software-rev");
    val.type = SR_STRING_T;
    val.data.string_val = swVersion;
    rc = sr_set_item(session, ds_cstr(&path), &val, 0);
    if (rc != SR_ERR_OK) {
        log_error("Set %s=%s failed: %s", ds_cstr(&path), val.data.string_val,
                  sr_strerror(rc));
    }

    ds_clear(&path);
    ds_put_format(&path, format, chassis_id, "serial-num");
    val.type = SR_STRING_T;
    val.data.string_val = "202102090245";
    rc = sr_set_item(session, ds_cstr(&path), &val, 0);
    if (rc != SR_ERR_OK) {
        log_error("Set %s=%s failed: %s", ds_cstr(&path), val.data.string_val,
                  sr_strerror(rc));
    }

    ds_clear(&path);
    ds_put_format(&path, format, chassis_id, "mfg-name");
    val.type = SR_STRING_T;
    val.data.string_val = "openil";
    rc = sr_set_item(session, ds_cstr(&path), &val, 0);
    if (rc != SR_ERR_OK) {
        log_error("Set %s=%s failed: %s", ds_cstr(&path), val.data.string_val,
                  sr_strerror(rc));
    }

    rc = sr_apply_changes(session, 0, 0);
    if (rc != SR_ERR_OK) {
        log_error("Apply failed when save chassis' information into operational: %s",
                  sr_strerror(rc));
        sr_discard_changes(session);
    }

    sr_session_switch_ds(session, SR_DS_RUNNING);

cleanup:
    if (NULL != hwVersion) {
        free(hwVersion);
    }

    if (NULL != swVersion) {
        free(swVersion);
    }

    if (NULL != chassis_id) {
        free(chassis_id);
    }

    ds_destroy(&path);
}