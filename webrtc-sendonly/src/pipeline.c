#include "headers/pipeline.h"
#include "headers/common.h"
#include "headers/custom_agent.h"
#include "headers/data_channel.h"
#include "headers/webrtc.h"
#include <gst/rtp/rtp.h>

#define STUN_SERVER "stun://stun.l.google.com:19302"
#define RTP_TWCC_URI                                                           \
  "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01"

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

  // Audio
  gchar *audio_desc =
      g_strdup_printf
      ("audiotestsrc is-live=true wave=red-noise ! audioconvert ! audioresample "
      "! queue ! opusenc ! rtpopuspay name=audiopay pt=%u",
      opus_pt);
  GstElement *audio_bin =
      gst_parse_bin_from_description (audio_desc, TRUE, NULL);
  g_free (audio_desc);

  // Video
  gchar *video_desc =
      g_strdup_printf
      ("videotestsrc is-live=true pattern=ball ! videoconvert ! queue ! "
      "vp8enc deadline=1 keyframe-max-dist=2000 ! rtpvp8pay name=videopay "
      "pt=%u",
      vp8_pt);
  GstElement *video_bin =
      gst_parse_bin_from_description (video_desc, TRUE, NULL);
  g_free (video_desc);

  // webrtcbin + custom ICE
  webrtc1 = gst_element_factory_make ("webrtcbin", "sendonly");
  g_object_set (webrtc1, "stun-server", STUN_SERVER, NULL);

  GstWebRTCICE *agent = GST_WEBRTC_ICE (customice_agent_new ("ice-agent"));
  g_object_set (webrtc1, "ice-agent", agent, NULL);
  g_object_unref (agent);

  gst_bin_add_many (GST_BIN (pipe1), audio_bin, video_bin, webrtc1, NULL);
  if (!gst_element_link (audio_bin, webrtc1))
    return FALSE;
  if (!gst_element_link (video_bin, webrtc1))
    return FALSE;

  // TWCC
  GstElement *videopay = gst_bin_get_by_name (GST_BIN (pipe1), "videopay");
  GstRTPHeaderExtension *twcc_ext =
      gst_rtp_header_extension_create_from_uri (RTP_TWCC_URI);
  gst_rtp_header_extension_set_id (twcc_ext, 1);
  g_signal_emit_by_name (videopay, "add-extension", twcc_ext);
  g_clear_object (&twcc_ext);
  g_clear_object (&videopay);

  // callbacks
  g_signal_connect (webrtc1, "on-negotiation-needed",
      G_CALLBACK (on_negotiation_needed), NULL);
  g_signal_connect (webrtc1, "on-ice-candidate", G_CALLBACK (on_ice_candidate),
      NULL);
  g_signal_connect (webrtc1, "on-data-channel", G_CALLBACK (on_data_channel),
      NULL);
  g_signal_connect (webrtc1, "notify::ice-gathering-state",
      G_CALLBACK (on_ice_gathering_state_notify), NULL);

  // create data channel
  g_signal_emit_by_name (webrtc1, "create-data-channel", "channel", NULL,
      &send_channel);
  if (send_channel) {
    g_print ("Created data channel\n");
    connect_data_channel_signals (send_channel);
  }
  // set state
  GstStateChangeReturn ret = gst_element_set_state (pipe1, GST_STATE_PLAYING);
  return ret != GST_STATE_CHANGE_FAILURE;
}
