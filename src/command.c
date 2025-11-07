#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"
#include "command.h"
#include "mqtt.h"

static int get_json_string(cJSON *obj, const char *key, const char **out) {
    cJSON *n = cJSON_GetObjectItemCaseSensitive(obj, key);

    if (!cJSON_IsString(n) || !n->valuestring) {
        return 0;
    }

    *out = n->valuestring;

    return 1;
}

void command_handle_set(const char *json, size_t len) {
    cJSON *root = cJSON_ParseWithLength(json, len);

    if (!root) {
        mqtt_publish_status("{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }

    const char *type = NULL;

    if(!get_json_string(root, "type", &type)) {
        mqtt_publish_status("{\"status\":\"error\",\"message\":\"Missing type\"}");
        cJSON_Delete(root);
        return;
    }

    char target[160] ={0};
    const char *image_path = NULL; //TODO: Populate later from assets.c
    const char *sound_path = NULL; //TDOD: Populate later from assests.c
    (void)image_path;
    (void)sound_path;

    if(type && strcmp(type, "holiday") == 0) {
      const char *name;
      if(!get_json_string(root, "name", &name)) {
        mqtt_publish_status("{\"status\":\"error\",\"message\":\"Missing name\"}");
        cJSON_Delete(root);
        return;
      }

      snprintf(target, sizeof(target), "%s", name ? name : "(NULL)");

      // TODO: resolve with assets.c for now simulate with success.
      image_path = "/assests/christmas/doorbell.png";
      sound_path = "/assests/christmas/doorbell.wav";
    }
    else if (type && strcmp(type, "custom") == 0) {
        const char *folder;
        if(!get_json_string(root, "folder", &folder)) {
            mqtt_publish_status("{\"status\":\"error\",\"message\":\"Missing folder\"}");
            cJSON_Delete(root);
            return;
        }

        snprintf(target, sizeof(target), "%s", folder ? folder : "(NULL)");

        // TODO: resolve with assets.c
        image_path = "/assets/custom/test/doorbell.png";
        sound_path = "/assets/custom/test/doorbell.wav";
    }
    else {
        mqtt_publish_status("{\"status\":\"error\",\"message\":\"Invalid type\"}");
        cJSON_Delete(root);
        return;
    }

    // Send uploading status
    {
        cJSON *status = cJSON_CreateObject();
        cJSON_AddStringToObject(status, "status", "uploading");
        cJSON_AddStringToObject(status, "message", target);

        char *txt = cJSON_PrintUnformatted(status);

        mqtt_publish_status(txt);
        free(txt);
        cJSON_Delete(status);
    }

    // TODO: perform SFTP upload here. For now just return success.
    int sftp_rc = 0;

    if (sftp_rc == 0) {
        cJSON *status = cJSON_CreateObject();
        cJSON_AddStringToObject(status, "status", "complete");
        cJSON_AddStringToObject(status, "last_set", target);

        char *txt = cJSON_PrintUnformatted(status);

        mqtt_publish_status(txt);
        free(txt);
        cJSON_Delete(status);
    }
    else {
        cJSON *status = cJSON_CreateObject();
        cJSON_AddStringToObject(status, "status", "error");
        cJSON_AddStringToObject(status, "error", "SFTP upload error");

        char *txt = cJSON_PrintUnformatted(status);

        mqtt_publish_status(txt);
        free(txt);
        cJSON_Delete(status);
    }

    cJSON_Delete(root);
}

void handle_set(const char *payload, size_t len) {
    command_handle_set(payload, len);
}
