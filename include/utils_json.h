#pragma once

#include "cJSON.h"
#include <stdbool.h>

/**
 * @brief Get a string value from a cJSON object
 * 
 * @param obj 
 * @param key 
 * @return const char* 
 */
const char *json_get_string(cJSON *obj, const char *key);

/**
 * @brief Duplicate and normalize a string value from a cJSON object
 * 
 * @param obj 
 * @param key 
 * @return char* 
 */
char *json_strdup_normalized(cJSON *obj, const char *key);

/**
 * @brief Get a boolean value from a cJSON object
 * 
 * @param obj 
 * @param key 
 * @param out 
 * @return true 
 * @return false 
 */
bool json_get_bool(cJSON *obj, const char *key, bool *out);

/**
 * @brief Get an integer value from a cJSON object
 * 
 * @param obj 
 * @param key 
 * @param out 
 * @return int 
 */
bool json_get_int(cJSON *obj, const char *key, int *out);

bool json_upsert_item_case_sensitive(cJSON *obj, const char *key, cJSON *new_item);

bool json_upsert_number_cs(cJSON *obj, const char *key, double v);

bool json_upsert_bool_cs(cJSON *obj, const char *key, bool v);

bool json_upsert_string_cs(cJSON *obj, const char *key, const char *v);