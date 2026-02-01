#pragma once

#include "config_types.h"

#include <stdbool.h>


void ha_topics_init(const config_t *cfg);

const char *ha_get_base_topic(void);

void ha_build_topic(char *out, size_t out_size, const char *subtopic);

const char *ha_availability_topic(void);

const char *ha_status_topic(void);

void ha_topic_subscribe_commands(void);
