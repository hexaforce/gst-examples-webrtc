#include "webrtc-common.h"

#define RTP_PAYLOAD_TYPE "96"
#define RTP_AUDIO_PAYLOAD_TYPE "97"
#define SOUP_HTTP_PORT 57778
#define STUN_SERVER "stun.l.google.com:19302"

#ifdef G_OS_WIN32
#define VIDEO_SRC "mfvideosrc"
#elif defined(__APPLE__)
#define VIDEO_SRC "avfvideosrc"
#else
#define VIDEO_SRC "v4l2src"
#endif

gchar *video_priority = NULL;
gchar *audio_priority = NULL;

GObject *send_channel, *receive_channel;

// GST_WEBRTC_PRIORITY_TYPE_VERY_LOW (1) – very-low
// GST_WEBRTC_PRIORITY_TYPE_LOW (2) – low
// GST_WEBRTC_PRIORITY_TYPE_MEDIUM (3) – medium
// GST_WEBRTC_PRIORITY_TYPE_HIGH (4) – high

void
set_sender_priority (GstWebRTCRTPTransceiver * trans, gchar * _priority)
{
  if (_priority) {
    GEnumClass *klass =
        (GEnumClass *) g_type_class_ref (GST_TYPE_WEBRTC_PRIORITY_TYPE);
    GEnumValue *en;
    GstWebRTCPriorityType priority = 0;

    g_return_if_fail (klass != NULL);
    en = g_enum_get_value_by_name (klass, _priority);
    if (!en) {
      en = g_enum_get_value_by_nick (klass, _priority);
    }
    g_type_class_unref (klass);

    if (en) {
      priority = en->value;

      GstWebRTCRTPSender *sender;
      g_object_get (trans, "sender", &sender, NULL);
      gst_webrtc_rtp_sender_set_priority (sender, priority);
      g_object_unref (sender);
    }
  }
}

void
on_data_channel (GstElement * webrtc, GObject * data_channel,
    gpointer user_data)
{
  connect_data_channel_signals (data_channel);
  receive_channel = data_channel;
}

void
soup_websocket_handler (G_GNUC_UNUSED SoupServer * server,
    SoupWebsocketConnection * connection, G_GNUC_UNUSED const char *path,
    G_GNUC_UNUSED SoupClientContext * client_context, gpointer user_data)
{
  ReceiverEntry *receiver_entry;
  GHashTable *receiver_entry_table = (GHashTable *) user_data;

  gst_print ("Processing new websocket connection %p", (gpointer) connection);

  g_signal_connect (G_OBJECT (connection), "closed",
      G_CALLBACK (soup_websocket_closed_cb), (gpointer) receiver_entry_table);

  GError *error;
  char *pipeline_desc;
  GstWebRTCRTPTransceiver *trans;
  GArray *transceivers;
  GstBus *bus;

  receiver_entry = g_new0 (ReceiverEntry, 1);
  receiver_entry->connection = connection;

  g_object_ref (G_OBJECT (connection));

  g_signal_connect (G_OBJECT (connection), "message",
      G_CALLBACK (soup_websocket_message_cb), (gpointer) receiver_entry);

  // "video/x-raw,width=1920,height=1080,framerate=30/1 ! "
  // "video/x-raw,width=1280,height=720,framerate=30/1 ! "
  // "video/x-raw,width=960,height=540,framerate=15/1 ! "
  // "video/x-raw,width=640,height=360,framerate=15/1 ! "

  // === pipeline config =============================
  pipeline_desc = g_strdup_printf (     //
      "webrtcbin name=webrtcbin " VIDEO_SRC " ! "
      "videorate ! "
      "videoscale ! "
      "video/x-raw,width=960,height=540,framerate=15/1 ! "
      "videoconvert ! "
      "queue max-size-buffers=1 ! "
      "vp8enc target-bitrate=600000 deadline=1 keyframe-max-dist=15 ! "
      "video/x-vp8 ! "
      "queue max-size-time=100000000 ! "
      "rtpvp8pay name=payloader ! "
      "application/x-rtp,media=video,encoding-name=VP8,payload="
      RTP_PAYLOAD_TYPE " ! " "webrtcbin. " "autoaudiosrc ! "
      "queue max-size-buffers=1 leaky=downstream ! " "audioconvert ! "
      "audioresample ! " "opusenc perfect-timestamp=true ! " "rtpopuspay pt="
      RTP_AUDIO_PAYLOAD_TYPE " ! " "application/x-rtp, encoding-name=OPUS ! "
      "webrtcbin. ");
  // pipeline_desc = g_strdup_printf( //
  //     "webrtcbin name=webrtcbin " VIDEO_SRC " ! "
  //     "videorate ! video/x-raw,framerate=30/1 ! "
  //     "videoscale ! video/x-raw,width=960,height=540 ! "
  //     "videoconvert ! video/x-raw,format=I420 ! "
  //     "queue max-size-buffers=10 max-size-bytes=0 max-size-time=100000000 leaky=downstream ! "
  //     "vp9enc target-bitrate=300000 deadline=1 keyframe-max-dist=15 cpu-used=8 ! "
  //     "rtpvp9pay name=payloader ! "
  //     "application/x-rtp,media=video,encoding-name=VP9,payload=" RTP_PAYLOAD_TYPE " ! "
  //     "webrtcbin. "
  //     "autoaudiosrc ! "
  //     "queue max-size-buffers=10 max-size-bytes=0 max-size-time=100000000 leaky=downstream ! "
  //     "audioconvert ! audioresample ! "
  //     "opusenc perfect-timestamp=true ! "
  //     "rtpopuspay pt=" RTP_AUDIO_PAYLOAD_TYPE " ! "
  //     "application/x-rtp,encoding-name=OPUS ! "
  //     "webrtcbin. ");
  // pipeline_desc = g_strdup_printf( //
  //     "webrtcbin name=webrtcbin " VIDEO_SRC " ! "
  //     "videorate ! "
  //     "videoscale ! "
  //     "video/x-raw,width=960,height=540,framerate=15/1 ! "
  //     "videoconvert ! "
  //     "queue max-size-buffers=1 ! "
  //     "vtenc_h264_hw realtime=true allow-frame-reordering=false max-keyframe-interval=15 qos=true ! "
  //     "video/x-h264, stream-format=avc, alignment=au ! "
  //     "queue ! "
  //     "h264parse config-interval=1 ! "
  //     "video/x-h264, level=(string)5.2 !"
  //     "rtph264pay config-interval=-1 name=payloader aggregate-mode=zero-latency ! "
  //     "application/x-rtp,media=video,encoding-name=H264,payload=" RTP_PAYLOAD_TYPE " ! "
  //     "webrtcbin. "
  //     "osxaudiosrc ! "
  //     "queue max-size-buffers=1 leaky=downstream ! "
  //     "audioconvert ! "
  //     "audioresample ! "
  //     "opusenc perfect-timestamp=true ! "
  //     "rtpopuspay pt=" RTP_AUDIO_PAYLOAD_TYPE " ! "
  //     "application/x-rtp, encoding-name=OPUS ! "
  //     "webrtcbin. ");

  error = NULL;
  receiver_entry->pipeline = gst_parse_launch (pipeline_desc, &error);
  if (error != NULL) {
    g_error ("Could not create WebRTC pipeline: %s\n", error->message);
    g_error_free (error);
    goto cleanup;
  }

  receiver_entry->webrtcbin =
      gst_bin_get_by_name (GST_BIN (receiver_entry->pipeline), "webrtcbin");
  g_assert_nonnull (receiver_entry->webrtcbin);
  // gst_util_set_object_arg(G_OBJECT(receiver_entry->webrtcbin), "use-media-library", TRUE);
  gst_util_set_object_arg (G_OBJECT (receiver_entry->webrtcbin),
      "bundle-policy", "max-bundle");
  gst_util_set_object_arg (G_OBJECT (receiver_entry->webrtcbin), "stun-server",
      "stun://" STUN_SERVER);

  // === transceiver config =============================

  g_signal_emit_by_name (receiver_entry->webrtcbin, "get-transceivers",
      &transceivers);
  g_assert (transceivers != NULL && transceivers->len > 1);
  trans = g_array_index (transceivers, GstWebRTCRTPTransceiver *, 0);
  g_object_set (trans, "direction",
      GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, NULL);
  set_sender_priority (trans, video_priority);

  trans = g_array_index (transceivers, GstWebRTCRTPTransceiver *, 1);
  g_object_set (trans, "direction",
      GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, NULL);
  set_sender_priority (trans, audio_priority);

  g_array_unref (transceivers);

  g_signal_connect (receiver_entry->webrtcbin, "on-negotiation-needed",
      G_CALLBACK (on_negotiation_needed_cb), (gpointer) receiver_entry);

  g_signal_connect (receiver_entry->webrtcbin, "on-ice-candidate",
      G_CALLBACK (on_ice_candidate_cb), (gpointer) receiver_entry);

  g_signal_connect (receiver_entry->webrtcbin, "notify::ice-gathering-state",
      G_CALLBACK (on_ice_gathering_state_notify), NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (receiver_entry->pipeline));
  gst_bus_add_watch (bus, bus_watch_cb, receiver_entry->pipeline);
  gst_object_unref (bus);

  gst_element_set_state (receiver_entry->pipeline, GST_STATE_READY);

  g_signal_emit_by_name (receiver_entry->webrtcbin, "create-data-channel",
      "sensor-reading-channel", NULL, &send_channel);
  if (send_channel) {
    gst_print ("Created data channel\n");
    connect_data_channel_signals (send_channel);
  } else {
    gst_print ("Could not create data channel, is usrsctp available?\n");
  }

  g_signal_connect (receiver_entry->webrtcbin, "on-data-channel",
      G_CALLBACK (on_data_channel), NULL);

  g_timeout_add (100, (GSourceFunc) webrtcbin_get_stats,
      receiver_entry->webrtcbin);

  if (gst_element_set_state (receiver_entry->pipeline,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    g_error ("Could not start pipeline");

  g_hash_table_replace (receiver_entry_table, connection, receiver_entry);
  return;

cleanup:
  destroy_receiver_entry ((gpointer) receiver_entry);
}

#if defined(G_OS_UNIX) || defined(__APPLE__)
gboolean
exit_sighandler (gpointer user_data)
{
  gst_print ("Caught signal, stopping mainloop\n");
  GMainLoop *mainloop = (GMainLoop *) user_data;
  g_main_loop_quit (mainloop);
  return TRUE;
}
#endif

static GOptionEntry entries[] = {
  {"video-priority", 0, 0, G_OPTION_ARG_STRING, &video_priority,
        "Priority of the video stream (very-low, low, medium or high)",
      "PRIORITY"},
  {"audio-priority", 0, 0, G_OPTION_ARG_STRING, &audio_priority,
        "Priority of the audio stream (very-low, low, medium or high)",
      "PRIORITY"},
  {NULL},
};

int
gst_main (int argc, char *argv[])
{
  GMainLoop *mainloop;
  SoupServer *soup_server;
  GHashTable *receiver_entry_table;
  GOptionContext *context;
  GError *error = NULL;

  setlocale (LC_ALL, "");
  gst_debug_set_default_threshold (GST_LEVEL_WARNING);

  context = g_option_context_new ("- gstreamer webrtc sendonly demo");
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_add_group (context, gst_init_get_option_group ());
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_printerr ("Error initializing: %s\n", error->message);
    return -1;
  }

  receiver_entry_table =
      g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
      destroy_receiver_entry);

  mainloop = g_main_loop_new (NULL, FALSE);
  g_assert_nonnull (mainloop);

#if defined(G_OS_UNIX) || defined(__APPLE__)
  g_unix_signal_add (SIGINT, exit_sighandler, mainloop);
  g_unix_signal_add (SIGTERM, exit_sighandler, mainloop);
#endif

  soup_server =
      soup_server_new (SOUP_SERVER_SERVER_HEADER, "webrtc-soup-server", NULL);
  soup_server_add_handler (soup_server, "/", soup_http_handler, NULL, NULL);
  soup_server_add_websocket_handler (soup_server, "/ws", NULL, NULL,
      soup_websocket_handler, (gpointer) receiver_entry_table, NULL);
  soup_server_listen_all (soup_server, SOUP_HTTP_PORT,
      (SoupServerListenOptions) 0, NULL);

  gst_print ("WebRTC page link: http://127.0.0.1:%d/\n", (gint) SOUP_HTTP_PORT);

  g_main_loop_run (mainloop);

  g_object_unref (G_OBJECT (soup_server));
  g_hash_table_destroy (receiver_entry_table);
  g_main_loop_unref (mainloop);

  gst_deinit ();

  return 0;
}

#ifdef __APPLE__
int
mac_main_function (int argc, char **argv, gpointer user_data)
{
  gst_init (&argc, &argv);
  return gst_main (argc, argv);
}
#endif

int
main (int argc, char *argv[])
{
#ifdef __APPLE__
  gst_macos_main (mac_main_function, argc, argv, NULL);
#else
  gst_init (&argc, &argv);
  return gst_main (argc, argv);
#endif
}
