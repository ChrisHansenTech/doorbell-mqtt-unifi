#include "utils_json.h"
#include "cJSON.h"

const char *json_get_string(cJSON *obj, const char *key) {
    if (!obj || !key) {
        return NULL;
    }

    cJSON *value = cJSON_GetObjectItemCaseSensitive(obj, key);
    return (cJSON_IsString(value) && value->valuestring) ? value->valuestring : NULL;
}


bool json_get_bool(cJSON *obj, const char *key, bool *out) {
    if (!obj || !key || !out) {
        return false;
    }

    cJSON *value = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsBool(value)) {
        *out = cJSON_IsTrue(value);
        return true;
    }

    return false;
}


bool json_get_int(cJSON *obj, const char *key, int *out) {
    if (!obj || !key || !out) {
        return false;
    }

    cJSON *value = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(value)) {
        *out = value->valueint;
        return true;
    }

    return false;
}

bool json_upsert_item_case_sensitive(cJSON *obj, const char *key, cJSON *new_item) {
    if (!obj || !cJSON_IsObject(obj) || !key || !new_item) {
        if (new_item) cJSON_Delete(new_item);
        return false;
    }

    cJSON *existing = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (existing) {
        if (!cJSON_ReplaceItemInObjectCaseSensitive(obj, key, new_item)) {
            cJSON_Delete(new_item);
            return false;
        }
        return true;
    }

    if (!cJSON_AddItemToObject(obj, key, new_item)) {
        cJSON_Delete(new_item);
        return false;
    }
    return true;
}

bool json_upsert_number_cs(cJSON *obj, const char *key, double v) {
    return json_upsert_item_case_sensitive(obj, key, cJSON_CreateNumber(v));
}

bool json_upsert_bool_cs(cJSON *obj, const char *key, bool v) {
    return json_upsert_item_case_sensitive(obj, key, cJSON_CreateBool(v));
}

bool json_upsert_string_cs(cJSON *obj, const char *key, const char *v) {
    return json_upsert_item_case_sensitive(obj, key, cJSON_CreateString(v ? v : ""));
}