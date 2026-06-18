#include "model.h"
#include "cJSON.h"
#include <string.h>

static status_model_t g_model = {0};

static void safe_strncpy(char *dst, const char *src, size_t n) {
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

bool model_parse(status_model_t *dst, const char *json, size_t len) {
    (void)len;
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;

    cJSON *item;

    item = cJSON_GetObjectItemCaseSensitive(root, "v");
    if (cJSON_IsNumber(item)) dst->v = (int)item->valuedouble;

    item = cJSON_GetObjectItemCaseSensitive(root, "hostname");
    if (cJSON_IsString(item)) safe_strncpy(dst->hostname, item->valuestring, sizeof(dst->hostname));

    item = cJSON_GetObjectItemCaseSensitive(root, "ips");
    if (cJSON_IsArray(item)) {
        dst->ip_count = 0;
        cJSON *ip;
        cJSON_ArrayForEach(ip, item) {
            if (cJSON_IsString(ip) && dst->ip_count < MODEL_MAX_IPS) {
                safe_strncpy(dst->ips[dst->ip_count++], ip->valuestring, MODEL_MAX_IP_LEN);
            }
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "primary_if");
    if (cJSON_IsString(item)) safe_strncpy(dst->primary_if, item->valuestring, sizeof(dst->primary_if));

    item = cJSON_GetObjectItemCaseSensitive(root, "link");
    if (cJSON_IsString(item)) safe_strncpy(dst->link, item->valuestring, sizeof(dst->link));

    item = cJSON_GetObjectItemCaseSensitive(root, "temp");
    if (cJSON_IsNumber(item)) dst->temp = (float)item->valuedouble;

    item = cJSON_GetObjectItemCaseSensitive(root, "cpu");
    if (cJSON_IsNumber(item)) dst->cpu = (float)item->valuedouble;

    item = cJSON_GetObjectItemCaseSensitive(root, "mem");
    if (cJSON_IsNumber(item)) dst->mem = (float)item->valuedouble;

    item = cJSON_GetObjectItemCaseSensitive(root, "disk");
    if (cJSON_IsNumber(item)) dst->disk = (float)item->valuedouble;

    item = cJSON_GetObjectItemCaseSensitive(root, "uptime");
    if (cJSON_IsString(item)) safe_strncpy(dst->uptime, item->valuestring, sizeof(dst->uptime));

    item = cJSON_GetObjectItemCaseSensitive(root, "net_tx");
    if (cJSON_IsNumber(item)) dst->net_tx = (float)item->valuedouble;

    item = cJSON_GetObjectItemCaseSensitive(root, "net_rx");
    if (cJSON_IsNumber(item)) dst->net_rx = (float)item->valuedouble;

    item = cJSON_GetObjectItemCaseSensitive(root, "alert");
    if (cJSON_IsBool(item)) dst->alert = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(root, "services");
    if (cJSON_IsArray(item)) {
        dst->service_count = 0;
        cJSON *svc;
        cJSON_ArrayForEach(svc, item) {
            if (dst->service_count >= MODEL_MAX_SERVICES) break;
            service_t *s = &dst->services[dst->service_count];
            cJSON *name = cJSON_GetObjectItemCaseSensitive(svc, "name");
            cJSON *active = cJSON_GetObjectItemCaseSensitive(svc, "active");
            if (cJSON_IsString(name) && cJSON_IsBool(active)) {
                safe_strncpy(s->name, name->valuestring, sizeof(s->name));
                s->active = cJSON_IsTrue(active);
                dst->service_count++;
            }
        }
    }

    cJSON_Delete(root);
    dst->valid = true;
    return true;
}

status_model_t *model_get(void) {
    return &g_model;
}
