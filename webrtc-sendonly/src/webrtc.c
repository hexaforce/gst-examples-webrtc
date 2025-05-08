#include "headers/common.h"
#include "headers/utils.h"

void
send_ice_candidate (GstElement * webrtc, guint mlineindex, gchar * candidate,
    gpointer user_data)
{
  JsonObject *ice = json_object_new ();
  json_object_set_string_member (ice, "candidate", candidate);
  json_object_set_int_member (ice, "sdpMLineIndex", mlineindex);

  JsonObject *msg = json_object_new ();
  json_object_set_object_member (msg, "candidate", ice);
  ws_send (ws_conn, RECEIVER_ICE, ws1Id, ws2Id, msg);
  json_object_unref (msg);
}

void
send_sdp_offer (GstWebRTCSessionDescription * desc)
{
  gchar *sdp = gst_sdp_message_as_text (desc->sdp);
  JsonObject *offer = json_object_new ();
  json_object_set_string_member (offer, "type", "offer");
  json_object_set_string_member (offer, "sdp", sdp);
  JsonObject *msg = json_object_new ();
  json_object_set_object_member (msg, "offer", offer);
  ws_send (ws_conn, RECEIVER_SDP_OFFER, ws1Id, ws2Id, msg);
  g_free (sdp);
  json_object_unref (msg);
}

void
on_ice_candidate (GstElement * webrtc, guint mlineindex, gchar * candidate,
    gpointer user_data)
{
  send_ice_candidate (webrtc, mlineindex, candidate, user_data);
}

void
on_ice_gathering_state_notify (GstElement * webrtc, GParamSpec * pspec,
    gpointer user_data)
{
  GstWebRTCICEGatheringState state;
  g_object_get (webrtc, "ice-gathering-state", &state, NULL);
  const gchar *state_str = "unknown";
  switch (state) {
    case GST_WEBRTC_ICE_GATHERING_STATE_NEW:
      state_str = "new";
      break;
    case GST_WEBRTC_ICE_GATHERING_STATE_GATHERING:
      state_str = "gathering";
      break;
    case GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE:
      state_str = "complete";
      break;
  }
  g_print ("ICE gathering state: %s\n", state_str);
}
