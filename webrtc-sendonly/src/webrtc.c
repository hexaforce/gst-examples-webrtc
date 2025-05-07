#include "headers/common.h"

void
send_ice_candidate (GstElement * webrtc, guint mlineindex,
    gchar * candidate, gpointer user_data)
{
  JsonObject *ice = json_object_new ();
  json_object_set_string_member (ice, "candidate", candidate);
  json_object_set_int_member (ice, "sdpMLineIndex", mlineindex);

  JsonObject *msg = json_object_new ();
  json_object_set_object_member (msg, "ice", ice);

  // 実際の送信処理は簡略化
  json_object_unref (msg);
}

void
send_sdp_offer (GstWebRTCSessionDescription * desc)
{
  gchar *sdp = gst_sdp_message_as_text (desc->sdp);

  JsonObject *offer = json_object_new ();
  json_object_set_string_member (offer, "type", "offer");
  json_object_set_string_member (offer, "sdp", sdp);

  // 実際の送信処理は簡略化
  g_free (sdp);
  json_object_unref (offer);
}
