#ifndef WEBRTC_H
#define WEBRTC_H

#include "common.h"

void send_ice_candidate (GstElement * webrtc,
    guint mlineindex, gchar * candidate, gpointer user_data);
void send_sdp_offer (GstWebRTCSessionDescription * desc);

#endif
