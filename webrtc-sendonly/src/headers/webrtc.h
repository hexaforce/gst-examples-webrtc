#ifndef WEBRTC_H
#define WEBRTC_H

#include <gst/gst.h>
#include <gst/sdp/sdp.h>
#include <gst/webrtc/webrtc.h>

void send_ice_candidate (GstElement * webrtc, guint mlineindex,
    gchar * candidate, gpointer user_data);

void send_sdp_offer (GstWebRTCSessionDescription * desc);

void on_ice_candidate (GstElement * webrtc, guint mlineindex, gchar * candidate,
    gpointer user_data);

void on_ice_gathering_state_notify (GstElement * webrtc, GParamSpec * pspec,
    gpointer user_data);

#endif
