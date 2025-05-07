#ifndef DATA_CHANNEL_H
#define DATA_CHANNEL_H

#include "common.h"

void on_data_channel (GstElement * webrtc,
    GObject * data_channel, gpointer user_data);
void setup_data_channel (GObject * data_channel);

#endif
