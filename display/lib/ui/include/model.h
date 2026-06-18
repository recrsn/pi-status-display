#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define MODEL_MAX_IPS           8
#define MODEL_MAX_IP_LEN        40   /* IPv6 worst case */
#define MODEL_MAX_HOSTNAME      64
#define MODEL_MAX_IFACE         16
#define MODEL_MAX_LINK          32
#define MODEL_MAX_UPTIME        32
#define MODEL_MAX_SERVICES      16
#define MODEL_MAX_SERVICE_NAME  48

typedef struct {
    char name[MODEL_MAX_SERVICE_NAME];
    bool active;
} service_t;

typedef struct {
    int     v;
    int64_t ts;
    char hostname[MODEL_MAX_HOSTNAME];
    char ips[MODEL_MAX_IPS][MODEL_MAX_IP_LEN];
    int  ip_count;
    char primary_if[MODEL_MAX_IFACE];
    char link[MODEL_MAX_LINK];
    float temp;
    float cpu;
    float mem;
    float disk;
    char  uptime[MODEL_MAX_UPTIME];
    float net_tx;
    float net_rx;
    bool  alert;
    service_t services[MODEL_MAX_SERVICES];
    int   service_count;

    bool     valid;           /* false until first successful parse */
    uint32_t last_update_ms;  /* for timeout detection */
} status_model_t;

/* Parse a newline-terminated JSON packet into dst. Returns true on success. */
bool model_parse(status_model_t *dst, const char *json, size_t len);

/* Singleton model shared across all screens. */
status_model_t *model_get(void);
