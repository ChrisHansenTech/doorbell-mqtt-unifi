#include "ha_mqtt.h"
#include "ha_discovery.h"
#include "ha_status.h"
#include "ha_topics.h"
#include "mqtt.h"

static const config_t *g_cfg = NULL;

static void ha_on_connect(bool reconnect, void *user)
{
    (void)reconnect;
    (void)user;

    ha_publish_discovery(g_cfg);

    status_set_availability(true);
    status_set_state("Online");
}

static void ha_on_disconnect(void *user)
{
    (void)user;

    status_set_availability(false);
}

bool ha_mqtt_bind(const config_t *cfg)
{
    g_cfg = cfg;

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