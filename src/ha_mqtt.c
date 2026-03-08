#include "ha_mqtt.h"
#include "errors.h"
#include "ha_discovery.h"
#include "ha_status.h"
#include "ha_topics.h"
#include "mqtt.h"
#include "state.h"
#include "state_types.h"

static const config_t *g_cfg = NULL;
static const char *g_state_dir = NULL;

static void ha_on_connect(bool reconnect, void *user)
{
    (void)reconnect;
    (void)user;

    ha_publish_discovery(g_cfg);
    ha_topic_subscribe_commands();

    applied_state_t last_applied = {0};
    if (state_load(&last_applied, g_state_dir) != ERROR_NONE) {
        LOG_WARN("Failed to load last_applied.json, state will not be restored");
    } else {
        status_set_last_applied_profile(last_applied.profile_name);

        if (last_applied.is_preset) {
            status_set_preset_selected(last_applied.profile_name);
            status_set_custom_directory("");
        } else {
            status_set_custom_directory("");
            status_set_preset_selected("none");
        }
    }

    status_set_availability(true);
}

static void ha_on_disconnect(void *user)
{
    (void)user;

    status_set_availability(false);
}

bool ha_mqtt_bind(const config_t *cfg, const char *state_dir)
{
    g_cfg = cfg;
    g_state_dir = state_dir;

    mqtt_set_last_will(
        ha_availability_topic(),
        "offline",
        cfg->mqtt_cfg.qos,
        cfg->mqtt_cfg.retained_online ? 1 : 0
    );

    mqtt_set_on_connect(ha_on_connect, NULL);
    mqtt_set_on_disconnect(ha_on_disconnect, NULL);

    return true;
}