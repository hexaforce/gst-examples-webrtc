#include "webrtc-common.h"

/* This example is a standalone app which serves a web page
 * and configures webrtcbin to receive an H.264 video feed, and to
 * send+recv an Opus audio stream */

#define RTP_PAYLOAD_TYPE "96"
#define RTP_AUDIO_PAYLOAD_TYPE "97"
#define SOUP_HTTP_PORT 57778
#define STUN_SERVER "stun.l.google.com:19302"

const gchar *html_source = " \n \
<html>\n \
  <head>\n \
    <script type='text/javascript'>\n \
      window.onload = () => {\n \
        var l = window.location\n \
        var ws = new WebSocket(`${l.protocol}ws`)\n \
        var conn\n \
        ws.onmessage = async (event) => {\n \
          try {\n \
            const { type, data } = JSON.parse(event.data)\n \
            if (!conn) {\n \
              conn = new RTCPeerConnection({ iceServers: [{ urls: 'stun:" STUN_SERVER "' }] })\n \
              conn.onicecandidate = (event) => event.candidate && ws.send(JSON.stringify({ type: 'ice', data: event.candidate }))\n \
            }\n \
            if (type == 'sdp') {\n \
              await conn.setRemoteDescription(data)\n \
              const stream = await navigator.mediaDevices.getUserMedia({ video: true, audio: true })\n \
              conn.addStream(stream)\n \
              const desc = await conn.createAnswer()\n \
              await conn.setLocalDescription(desc)\n \
              ws.send(JSON.stringify({ type: 'sdp', data: conn.localDescription }))\n \
            } else if (type == 'ice') {\n \
              await conn.addIceCandidate(new RTCIceCandidate(data))\n \
            }\n \
          } catch (err) {\n \
            console.error(err)\n \
          }\n \
        }\n \
      }\n \
    </script>\n \
  </head>\n \
  <body>\n \
  </body>\n \
</html>\n \
";

static void
handle_media_stream (GstPad * pad, GstElement * pipe, const char *convert_name,
    const char *sink_name)
{
  GstPad *qpad;
  GstElement *q, *conv, *resample, *sink;
  GstPadLinkReturn ret;

  gst_print ("Trying to handle stream with %s ! %s", convert_name, sink_name);

  q = gst_element_factory_make ("queue", NULL);
  g_assert_nonnull (q);
  conv = gst_element_factory_make (convert_name, NULL);
  g_assert_nonnull (conv);
  sink = gst_element_factory_make (sink_name, NULL);
  g_assert_nonnull (sink);

  if (g_strcmp0 (convert_name, "audioconvert") == 0) {
    /* Might also need to resample, so add it just in case.
     * Will be a no-op if it's not required. */
    resample = gst_element_factory_make ("audioresample", NULL);
    g_assert_nonnull (resample);
    gst_bin_add_many (GST_BIN (pipe), q, conv, resample, sink, NULL);
    gst_element_sync_state_with_parent (q);
    gst_element_sync_state_with_parent (conv);
    gst_element_sync_state_with_parent (resample);
    gst_element_sync_state_with_parent (sink);
    gst_element_link_many (q, conv, resample, sink, NULL);
  } else {
    gst_bin_add_many (GST_BIN (pipe), q, conv, sink, NULL);
    gst_element_sync_state_with_parent (q);
    gst_element_sync_state_with_parent (conv);
    gst_element_sync_state_with_parent (sink);
    gst_element_link_many (q, conv, sink, NULL);
  }

  qpad = gst_element_get_static_pad (q, "sink");

  ret = gst_pad_link (pad, qpad);
  g_assert_cmphex (ret, ==, GST_PAD_LINK_OK);
}

static void
on_incoming_decodebin_stream (GstElement * decodebin, GstPad * pad,
    GstElement * pipe)
{
  GstCaps *caps;
  const gchar *name;

  if (!gst_pad_has_current_caps (pad)) {
    gst_printerr ("Pad '%s' has no caps, can't do anything, ignoring\n",
        GST_PAD_NAME (pad));
    return;
  }

  caps = gst_pad_get_current_caps (pad);
  name = gst_structure_get_name (gst_caps_get_structure (caps, 0));

  if (g_str_has_prefix (name, "video")) {
    handle_media_stream (pad, pipe, "videoconvert", "autovideosink");
  } else if (g_str_has_prefix (name, "audio")) {
    handle_media_stream (pad, pipe, "audioconvert", "autoaudiosink");
  } else {
    gst_printerr ("Unknown pad %s, ignoring", GST_PAD_NAME (pad));
  }
}

static void
on_incoming_stream (GstElement * webrtc, GstPad * pad,
    ReceiverEntry * receiver_entry)
{
  GstElement *decodebin;
  GstPad *sinkpad;

  if (GST_PAD_DIRECTION (pad) != GST_PAD_SRC)
    return;

  decodebin = gst_element_factory_make ("decodebin", NULL);
  g_signal_connect (decodebin, "pad-added",
      G_CALLBACK (on_incoming_decodebin_stream), receiver_entry->pipeline);
  gst_bin_add (GST_BIN (receiver_entry->pipeline), decodebin);
  gst_element_sync_state_with_parent (decodebin);

  sinkpad = gst_element_get_static_pad (decodebin, "sink");
  gst_pad_link (pad, sinkpad);
  gst_object_unref (sinkpad);
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
  GstWebRTCRTPTransceiver *trans;
  GstCaps *video_caps;
  GstBus *bus;

  receiver_entry = g_new0 (ReceiverEntry, 1);
  receiver_entry->connection = connection;

  g_object_ref (G_OBJECT (connection));

  g_signal_connect (G_OBJECT (connection), "message",
      G_CALLBACK (soup_websocket_message_cb), (gpointer) receiver_entry);

  // === pipeline config =============================
  error = NULL;
  receiver_entry->pipeline = gst_parse_launch ( //
      "webrtcbin name=webrtcbin stun-server=stun://" STUN_SERVER " "
      "audiotestsrc is-live=true wave=red-noise ! "
      "audioconvert ! "
      "audioresample ! "
      "queue ! "
      "opusenc perfect-timestamp=true ! "
      "rtpopuspay ! "
      "queue ! "
      "application/x-rtp,media=audio,encoding-name=OPUS,payload="
      RTP_AUDIO_PAYLOAD_TYPE " ! " "webrtcbin. ", &error);
  if (error != NULL) {
    g_error ("Could not create WebRTC pipeline: %s\n", error->message);
    g_error_free (error);
    goto cleanup;
  }

  receiver_entry->webrtcbin =
      gst_bin_get_by_name (GST_BIN (receiver_entry->pipeline), "webrtcbin");
  g_assert (receiver_entry->webrtcbin != NULL);

  // === transceiver config =============================

  /* Incoming streams will be exposed via this signal */
  g_signal_connect (receiver_entry->webrtcbin, "pad-added",
      G_CALLBACK (on_incoming_stream), receiver_entry);

#if 0
  GstElement *rtpbin =
      gst_bin_get_by_name (GST_BIN (receiver_entry->webrtcbin), "rtpbin");
  g_object_set (rtpbin, "latency", 40, NULL);
  gst_object_unref (rtpbin);
#endif

  // Create a 2nd transceiver for the receive only video stream
  video_caps =
      gst_caps_from_string
      ("application/x-rtp,media=video,encoding-name=H264,payload="
      RTP_PAYLOAD_TYPE
      ",clock-rate=90000,packetization-mode=(string)1, profile-level-id=(string)42c016");
  g_signal_emit_by_name (receiver_entry->webrtcbin, "add-transceiver",
      GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, video_caps, &trans);
  gst_caps_unref (video_caps);
  gst_object_unref (trans);

  g_signal_connect (receiver_entry->webrtcbin, "on-negotiation-needed",
      G_CALLBACK (on_negotiation_needed_cb), (gpointer) receiver_entry);

  g_signal_connect (receiver_entry->webrtcbin, "on-ice-candidate",
      G_CALLBACK (on_ice_candidate_cb), (gpointer) receiver_entry);

  bus = gst_pipeline_get_bus (GST_PIPELINE (receiver_entry->pipeline));
  gst_bus_add_watch (bus, bus_watch_cb, receiver_entry->pipeline);
  gst_object_unref (bus);

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

int
gst_main (int argc, char *argv[])
{
  GMainLoop *mainloop;
  SoupServer *soup_server;
  GHashTable *receiver_entry_table;

  setlocale (LC_ALL, "");

  receiver_entry_table =
      g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
      destroy_receiver_entry);

  mainloop = g_main_loop_new (NULL, FALSE);
  g_assert (mainloop != NULL);

#if defined(G_OS_UNIX) || defined(__APPLE__)
  g_unix_signal_add (SIGINT, exit_sighandler, mainloop);
  g_unix_signal_add (SIGTERM, exit_sighandler, mainloop);
#endif

  soup_server =
      soup_server_new (SOUP_SERVER_SERVER_HEADER, "webrtc-soup-server", NULL);
  soup_server_add_handler (soup_server, "/", soup_http_handler,
      (gpointer) html_source, NULL);
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
