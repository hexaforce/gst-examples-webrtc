#ifndef DATA_CHANNEL_H
#define DATA_CHANNEL_H

#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>

void setup_data_channel (GObject * data_channel);

void on_data_channel (GstElement * webrtc, GObject * data_channel,
    gpointer user_data);

void connect_data_channel_signals (GObject * dc);

#endif
