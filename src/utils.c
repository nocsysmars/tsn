#include "utils.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <netinet/in.h>

#include "log.h"

#define TIME_BUF_SIZE 32

char* get_ieee_mac_addr(const char *mac)
{
    size_t len = strlen(mac);
    char *buf = malloc((len + 1) * sizeof(char));
    const char *c = mac;

    size_t i = 0;
    while (*c != '\0') {
        if (*c == ':') {
            buf[i++] = '-';
        } else {
            buf[i++] = *c;
        }
        c++;
    }
    buf[i] = '\0';
    return buf;
}

char *to_ieee_mac_addr(char *mac)
{
    char *c = mac;

    while (*c != '\0') {
        if (*c == ':') {
            *c = '-';
        }

        c++;
    }

    return mac;
}

char *get_iso8601_time()
{
    char *buf = NULL;
    time_t rawtime = time(NULL);
    if (rawtime == -1) {
        log_error("Get raw time using time() function failed");
        return NULL;
    }

    struct tm *ptm = localtime(&rawtime);
    if (ptm == NULL) {
        log_error("Execute localtime() function failed");
        return NULL;
    }

    buf = malloc(TIME_BUF_SIZE * sizeof(char));
    strftime(buf, TIME_BUF_SIZE, "%Y-%m-%dT%TZ", ptm);
    return buf;
}


uint8_t netmask_prefix(struct sockaddr *netmask)
{
    uint8_t prefix = 0;

    if (netmask->sa_family == AF_INET) {
        struct sockaddr_in *addr = (struct sockaddr_in*)netmask;
        uint32_t in_addr = addr->sin_addr.s_addr;
        while(in_addr != 0) {
            if (in_addr & 1 == 1) {
                prefix++;
            }
            in_addr >>= 1;
        }
    } else {
        struct sockaddr_in6 *addr = (struct sockaddr_in6*)netmask;
        for (int i = 0; i < 4; i++) {
            uint32_t in_addr = addr->sin6_addr.s6_addr32[i];
            while (in_addr != 0) {
                if (in_addr & 1 == 1) {
                    prefix++;
                }
                in_addr >>= 1;
            }
        }
    }

    return prefix;
}

static uint64_t age_str2num(char *age_str)
{
    uint64_t age = 0;
    char *c = age_str;
    char *end;

    end = strstr(c, "day,");
    if (end == NULL) {
        log_error("Parse age failed");
        return age;
    }

    *end = '\0';
    age += atoi(c) * 24;
    c = end + 4;

    for (int i = 0; i < 2; i++) {
        end = strstr(c, ":");
        if (end == NULL) {
            log_error("Parse age failed");
            return age;
        }

        *end = '\0';
        age += (age + atoi(c)) * 60;
        c = end + 1;
    }

    age += atoi(c);
    return age;
}

uint64_t get_age(char *age_str)
{
    time_t rawtime = time(NULL);
    if (rawtime == -1) {
        log_error("Get raw time using time() function failed");
        return 0;
    }

    return (uint64_t)rawtime - age_str2num(age_str);
}