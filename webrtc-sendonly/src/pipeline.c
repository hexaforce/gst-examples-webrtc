#include "headers/common.h"
#include "headers/webrtc.h"

static void
on_offer_created (GstPromise * promise, gpointer user_data)
{
  GstWebRTCSessionDescription *offer = NULL;
  const GstStructure *reply = gst_promise_get_reply (promise);
  gst_structure_get (reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION,
      &offer, NULL);

  GstPromise *local_desc_promise = gst_promise_new ();
  g_signal_emit_by_name (webrtc1, "set-local-description", offer,
      local_desc_promise);
  gst_promise_interrupt (local_desc_promise);
  gst_promise_unref (local_desc_promise);

  send_sdp_offer (offer);
  gst_webrtc_session_description_free (offer);
}

void
on_negotiation_needed (GstElement * element, gpointer user_data)
{
  app_state = PEER_CALL_NEGOTIATING;
  GstPromise *promise =
      gst_promise_new_with_change_func (on_offer_created, NULL, NULL);
  g_signal_emit_by_name (webrtc1, "create-offer", NULL, promise);
}

gboolean
start_pipeline (guint opus_pt, guint vp8_pt)
{
  pipe1 = gst_pipeline_new ("webrtc-pipeline");
  webrtc1 = gst_element_factory_make ("webrtcbin", "webrtc");

  GstElement *audiotestsrc = gst_element_factory_make ("audiotestsrc", "audio");
  GstElement *videotestsrc = gst_element_factory_make ("videotestsrc", "video");

  gst_bin_add_many (GST_BIN (pipe1), audiotestsrc, videotestsrc, webrtc1, NULL);

  // 簡略化のためパイプラインリンク部分は省略
  g_signal_connect (webrtc1, "on-negotiation-needed",
      G_CALLBACK (on_negotiation_needed), NULL);

  return gst_element_set_state (pipe1,
      GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE;
}
