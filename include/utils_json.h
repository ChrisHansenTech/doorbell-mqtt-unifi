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

/**
 * @brief Get a double value from a cJSON object
 * 
 * @param obj 
 * @param key 
 * @param out 
 * @return true 
 * @return false 
 */
bool json_get_double(cJSON *obj, const char *key, double *out);

/**
 * @brief Upsert an item into a cJSON object with case-sensitive key matching. 
 *        If the key already exists, it will be replaced. If it does not exist, 
 *        it will be added. The new_item will be deleted if the operation fails.
 * 
 * @param obj 
 * @param key 
 * @param new_item 
 * @return true 
 * @return false 
 */
bool json_upsert_item_case_sensitive(cJSON *obj, const char *key, cJSON *new_item);

/**
 * @brief Upsert a number value into a cJSON object with case-sensitive key matching. 
 *        This is a convenience wrapper around json_upsert_item_case_sensitive.
 * 
 * @param obj 
 * @param key 
 * @param v 
 * @return true 
 * @return false 
 */
bool json_upsert_number_cs(cJSON *obj, const char *key, double v);

/**
 * @brief Upsert a boolean value into a cJSON object with case-sensitive key matching.
 * 
 * @param obj 
 * @param key 
 * @param v 
 * @return true 
 * @return false 
 */
bool json_upsert_bool_cs(cJSON *obj, const char *key, bool v);

/**
 * @brief Upsert a string value into a cJSON object with case-sensitive key matching.
 * 
 * @param obj 
 * @param key 
 * @param v 
 * @return true 
 * @return false 
 */
bool json_upsert_string_cs(cJSON *obj, const char *key, const char *v);