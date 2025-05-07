/*
 * Demo gstreamer app for negotiating and streaming a sendonly webrtc stream
 * with a browser JS app.
 *
 * Build by running: `make webrtc-sendonly`, or build the gstreamer monorepo.
 *
 * Author: Nirbheek Chauhan <nirbheek@centricular.com>
 */
#include <gst/gst.h>
#include <gst/sdp/sdp.h>
#include <gst/rtp/rtp.h>

#include <gst/webrtc/webrtc.h>
#include <gst/webrtc/nice/nice.h>

#include "custom_agent.h"

 /* For signalling */
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#include <string.h>
#include "utils.h"

#ifdef G_OS_WIN32
#define VIDEO_SRC "mfvideosrc"
#define AUDIO_SRC "wasapisrc"
#define DEVICE_PROP "device"
#elif defined(__APPLE__)
#define VIDEO_SRC "avfvideosrc"
#define AUDIO_SRC "avfaudiosrc"
#define DEVICE_PROP "device-index"
#else
#define VIDEO_SRC "v4l2src"
#define AUDIO_SRC "pulsesrc"
#define DEVICE_PROP "device"
#endif

enum AppState
{
  APP_STATE_UNKNOWN = 0,
  APP_STATE_ERROR = 1,          /* generic error */
  SERVER_CONNECTING = 1000,
  SERVER_CONNECTION_ERROR,
  SERVER_CONNECTED,             /* Ready to register */
  SERVER_REGISTERING = 2000,
  SERVER_REGISTRATION_ERROR,
  SERVER_REGISTERED,            /* Ready to call a peer */
  SERVER_CLOSED,                /* server connection closed by us or the server */
  PEER_CONNECTING = 3000,
  PEER_CONNECTION_ERROR,
  PEER_CONNECTED,
  PEER_CALL_NEGOTIATING = 4000,
  PEER_CALL_STARTED,
  PEER_CALL_STOPPING,
  PEER_CALL_STOPPED,
  PEER_CALL_ERROR,
};

#define GST_CAT_DEFAULT webrtc_sendonly_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static GMainLoop *loop;
static GstElement *pipe1, *webrtc1, *audio_bin, *video_bin = NULL;
static GObject *send_channel, *receive_channel;

static SoupWebsocketConnection *ws_conn = NULL;
static enum AppState app_state = 0;
static const gchar *server_url = "ws://192.168.151.5:8443/signaling";

static gchar *ws1Id = NULL;
static gchar *ws2Id = NULL;

static gboolean
cleanup_and_quit_loop (const gchar * msg, enum AppState state)
{
  gst_print ("-------- cleanup_and_quit_loop\n");
  if (msg)
    gst_printerr ("%s\n", msg);
  if (state > 0)
    app_state = state;

  if (ws_conn) {
    if (soup_websocket_connection_get_state (ws_conn) ==
        SOUP_WEBSOCKET_STATE_OPEN)
      soup_websocket_connection_close (ws_conn, 1000, "");
    else
      g_clear_object (&ws_conn);
  }

  if (loop) {
    g_main_loop_quit (loop);
    g_clear_pointer (&loop, g_main_loop_unref);
  }

  return G_SOURCE_REMOVE;
}

static void
send_ice_candidate_message (GstElement * webrtc G_GNUC_UNUSED, guint mlineindex,
    gchar * candidate, gpointer user_data G_GNUC_UNUSED)
{
  JsonObject *ice, *msg;

  if (app_state < PEER_CALL_NEGOTIATING) {
    cleanup_and_quit_loop ("Can't send ICE, not in call", APP_STATE_ERROR);
    return;
  }

  ice = json_object_new ();
  json_object_set_string_member (ice, "candidate", candidate);
  json_object_set_int_member (ice, "sdpMLineIndex", mlineindex);
  msg = json_object_new ();
  json_object_set_object_member (msg, "candidate", ice);
  ws_send (ws_conn, RECEIVER_ICE, ws1Id, ws2Id, msg);
  json_object_unref (msg);
}

static void
send_sdp_to_peer (GstWebRTCSessionDescription * desc)
{
  gchar *sdp_text;
  JsonObject *msg, *sdp;

  if (app_state < PEER_CALL_NEGOTIATING) {
    cleanup_and_quit_loop ("Can't send SDP to peer, not in call",
        APP_STATE_ERROR);
    return;
  }

  sdp = json_object_new ();
  json_object_set_string_member (sdp, "type", "offer");
  sdp_text = gst_sdp_message_as_text (desc->sdp);
  json_object_set_string_member (sdp, "sdp", sdp_text);
  ws_send (ws_conn, RECEIVER_SDP_OFFER, ws1Id, ws2Id, sdp);
  json_object_unref (msg);
}

 /* Offer created by our pipeline, to be sent to the peer */
static void
on_offer_created (GstPromise * promise, gpointer user_data)
{
  gst_print ("-------- on_offer_created\n");
  GstWebRTCSessionDescription *offer = NULL;
  const GstStructure *reply;

  g_assert_cmphex (app_state, ==, PEER_CALL_NEGOTIATING);

  g_assert_cmphex (gst_promise_wait (promise), ==, GST_PROMISE_RESULT_REPLIED);
  reply = gst_promise_get_reply (promise);
  gst_structure_get (reply, "offer",
      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);
  gst_promise_unref (promise);

  promise = gst_promise_new ();
  g_signal_emit_by_name (webrtc1, "set-local-description", offer, promise);
  gst_promise_interrupt (promise);
  gst_promise_unref (promise);

  send_sdp_to_peer (offer);
  gst_webrtc_session_description_free (offer);
}

static void
on_negotiation_needed (GstElement * element)
{
  gst_print ("-------- on_negotiation_needed\n");
  app_state = PEER_CALL_NEGOTIATING;

  GstPromise *promise =
      gst_promise_new_with_change_func (on_offer_created, NULL, NULL);
  g_signal_emit_by_name (webrtc1, "create-offer", NULL, promise);
}

static void
data_channel_on_error (GObject * dc, gpointer user_data)
{
  cleanup_and_quit_loop ("Data channel error", 0);
}

static void
data_channel_on_open (GObject * dc, gpointer user_data)
{
  gst_print ("-------- data_channel_on_open\n");
  GBytes *bytes = g_bytes_new ("data", strlen ("data"));
  gst_print ("data channel opened\n");
  g_signal_emit_by_name (dc, "send-string", "Hi! from GStreamer");
  g_signal_emit_by_name (dc, "send-data", bytes);
  g_bytes_unref (bytes);
}

static void
data_channel_on_close (GObject * dc, gpointer user_data)
{
  cleanup_and_quit_loop ("Data channel closed", 0);
}

static void
data_channel_on_message_string (GObject * dc, gchar * str, gpointer user_data)
{
  gst_print ("Received data channel message: %s\n", str);
}

static void
connect_data_channel_signals (GObject * data_channel)
{
  gst_print ("-------- connect_data_channel_signals\n");
  g_signal_connect (data_channel, "on-error",
      G_CALLBACK (data_channel_on_error), NULL);
  g_signal_connect (data_channel, "on-open", G_CALLBACK (data_channel_on_open),
      NULL);
  g_signal_connect (data_channel, "on-close",
      G_CALLBACK (data_channel_on_close), NULL);
  g_signal_connect (data_channel, "on-message-string",
      G_CALLBACK (data_channel_on_message_string), NULL);
}

static void
on_data_channel (GstElement * webrtc, GObject * data_channel,
    gpointer user_data)
{
  gst_print ("-------- on_data_channel\n");
  connect_data_channel_signals (data_channel);
  receive_channel = data_channel;
}

static void
on_ice_gathering_state_notify (GstElement * webrtcbin, GParamSpec * pspec,
    gpointer user_data)
{
  gst_print ("-------- on_ice_gathering_state_notify\n");
  GstWebRTCICEGatheringState ice_gather_state;
  const gchar *new_state = "unknown";

  g_object_get (webrtcbin, "ice-gathering-state", &ice_gather_state, NULL);
  switch (ice_gather_state) {
    case GST_WEBRTC_ICE_GATHERING_STATE_NEW:
      new_state = "new";
      break;
    case GST_WEBRTC_ICE_GATHERING_STATE_GATHERING:
      new_state = "gathering";
      break;
    case GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE:
      new_state = "complete";
      break;
  }
  gst_print ("ICE gathering state changed to %s\n", new_state);
}

static gboolean webrtcbin_get_stats (GstElement * webrtcbin);

static gboolean
on_webrtcbin_stat (const GstIdStr * fieldname, const GValue * value,
    gpointer unused)
{
  // gst_print ("-------- on_webrtcbin_stat\n");
  if (GST_VALUE_HOLDS_STRUCTURE (value)) {
    GST_DEBUG ("stat: \'%s\': %" GST_PTR_FORMAT, gst_id_str_as_str (fieldname),
        gst_value_get_structure (value));
  } else {
    GST_FIXME ("unknown field \'%s\' value type: \'%s\'",
        gst_id_str_as_str (fieldname), g_type_name (G_VALUE_TYPE (value)));
  }

  return TRUE;
}

static void
on_webrtcbin_get_stats (GstPromise * promise, GstElement * webrtcbin)
{
  // gst_print ("-------- on_webrtcbin_get_stats\n");
  const GstStructure *stats;

  g_return_if_fail (gst_promise_wait (promise) == GST_PROMISE_RESULT_REPLIED);

  stats = gst_promise_get_reply (promise);
  gst_structure_foreach_id_str (stats, on_webrtcbin_stat, NULL);

  g_timeout_add (100, (GSourceFunc) webrtcbin_get_stats, webrtcbin);
}

static gboolean
webrtcbin_get_stats (GstElement * webrtcbin)
{
  // gst_print ("-------- webrtcbin_get_stats\n");
  GstPromise *promise;

  promise =
      gst_promise_new_with_change_func (
      (GstPromiseChangeFunc) on_webrtcbin_get_stats, webrtcbin, NULL);

  GST_TRACE ("emitting get-stats on %" GST_PTR_FORMAT, webrtcbin);
  g_signal_emit_by_name (webrtcbin, "get-stats", NULL, promise);
  gst_promise_unref (promise);

  return G_SOURCE_REMOVE;
}

static gboolean
bus_watch_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  GstPipeline *pipeline = user_data;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ASYNC_DONE:
    {
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe1),
          GST_DEBUG_GRAPH_SHOW_ALL, "webrtc-sendonly.async-done");
      break;
    }
    case GST_MESSAGE_ERROR:
    {
      GError *error = NULL;
      gchar *debug = NULL;

      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe1),
          GST_DEBUG_GRAPH_SHOW_ALL, "webrtc-sendonly.error");

      gst_message_parse_error (message, &error, &debug);
      cleanup_and_quit_loop ("ERROR: Error on bus", APP_STATE_ERROR);
      g_warning ("Error on bus: %s (debug: %s)", error->message, debug);
      g_error_free (error);
      g_free (debug);
      break;
    }
    case GST_MESSAGE_WARNING:
    {
      GError *error = NULL;
      gchar *debug = NULL;

      gst_message_parse_warning (message, &error, &debug);
      g_warning ("Warning on bus: %s (debug: %s)", error->message, debug);
      g_error_free (error);
      g_free (debug);
      break;
    }
    case GST_MESSAGE_LATENCY:
      gst_bin_recalculate_latency (GST_BIN (pipeline));
      break;
    default:
      break;
  }

  return G_SOURCE_CONTINUE;
}

#define STUN_SERVER "stun://stun.l.google.com:19302"
#define RTP_TWCC_URI "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01"
#define RTP_OPUS_DEFAULT_PT 97
#define RTP_VP8_DEFAULT_PT 96

static gboolean
start_pipeline (guint opus_pt, guint vp8_pt)
{
  gst_print ("-------- start_pipeline\n");
  GstBus *bus;
  char *audio_desc, *video_desc;
  GstStateChangeReturn ret;
  GstWebRTCICE *custom_agent;
  GError *audio_error = NULL;
  GError *video_error = NULL;

  pipe1 = gst_pipeline_new ("webrtc-pipeline");

  audio_desc =
      g_strdup_printf
      ("audiotestsrc is-live=true wave=red-noise ! audioconvert ! audioresample"
      "! queue ! opusenc perfect-timestamp=true ! rtpopuspay name=audiopay pt=%u "
      "! application/x-rtp, encoding-name=OPUS ! queue", opus_pt);
  audio_bin = gst_parse_bin_from_description (audio_desc, TRUE, &audio_error);
  g_free (audio_desc);
  if (audio_error) {
    gst_printerr ("Failed to parse audio_bin: %s\n", audio_error->message);
    g_error_free (audio_error);
    goto err;
  }

  video_desc =
      g_strdup_printf
      ("videotestsrc is-live=true pattern=ball ! videoconvert ! queue ! "
      /* increase the default keyframe distance, browsers have really long
       * periods between keyframes and rely on PLI events on packet loss to
       * fix corrupted video.
       */
      "vp8enc deadline=1 keyframe-max-dist=2000 ! "
      /* picture-id-mode=15-bit seems to make TWCC stats behave better, and
       * fixes stuttery video playback in Chrome */
      "rtpvp8pay name=videopay picture-id-mode=15-bit pt=%u ! queue", vp8_pt);
  video_bin = gst_parse_bin_from_description (video_desc, TRUE, &video_error);
  g_free (video_desc);
  if (video_error) {
    gst_printerr ("Failed to parse video_bin: %s\n", video_error->message);
    g_error_free (video_error);
    goto err;
  }

  webrtc1 = gst_element_factory_make_full ("webrtcbin", "name", "sendonly",
      "stun-server", STUN_SERVER, NULL);

  g_assert_nonnull (webrtc1);
  gst_util_set_object_arg (G_OBJECT (webrtc1), "bundle-policy", "max-bundle");

  gst_bin_add_many (GST_BIN (pipe1), audio_bin, video_bin, webrtc1, NULL);

  if (!gst_element_link (audio_bin, webrtc1)) {
    gst_printerr ("Failed to link audio_bin \n");
  }
  if (!gst_element_link (video_bin, webrtc1)) {
    gst_printerr ("Failed to link video_bin \n");
  }

  GstElement *videopay, *audiopay;
  GstRTPHeaderExtension *video_twcc, *audio_twcc;

  videopay = gst_bin_get_by_name (GST_BIN (pipe1), "videopay");
  g_assert_nonnull (videopay);
  video_twcc = gst_rtp_header_extension_create_from_uri (RTP_TWCC_URI);
  g_assert_nonnull (video_twcc);
  gst_rtp_header_extension_set_id (video_twcc, 1);
  g_signal_emit_by_name (videopay, "add-extension", video_twcc);
  g_clear_object (&video_twcc);
  g_clear_object (&videopay);

  audiopay = gst_bin_get_by_name (GST_BIN (pipe1), "audiopay");
  g_assert_nonnull (audiopay);
  audio_twcc = gst_rtp_header_extension_create_from_uri (RTP_TWCC_URI);
  g_assert_nonnull (audio_twcc);
  gst_rtp_header_extension_set_id (audio_twcc, 1);
  g_signal_emit_by_name (audiopay, "add-extension", audio_twcc);
  g_clear_object (&audio_twcc);
  g_clear_object (&audiopay);

  g_signal_connect (webrtc1, "on-negotiation-needed",
      G_CALLBACK (on_negotiation_needed), NULL);
  g_signal_connect (webrtc1, "on-ice-candidate",
      G_CALLBACK (send_ice_candidate_message), NULL);
  g_signal_connect (webrtc1, "notify::ice-gathering-state",
      G_CALLBACK (on_ice_gathering_state_notify), NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipe1));
  gst_bus_add_watch (bus, bus_watch_cb, pipe1);
  gst_object_unref (bus);

  gst_element_set_state (pipe1, GST_STATE_READY);

  g_signal_emit_by_name (webrtc1, "create-data-channel", "channel", NULL,
      &send_channel);
  if (send_channel) {
    gst_print ("Created data channel\n");
    connect_data_channel_signals (send_channel);
  } else {
    gst_print ("Could not create data channel, is usrsctp available?\n");
  }

  g_signal_connect (webrtc1, "on-data-channel", G_CALLBACK (on_data_channel),
      NULL);

  g_timeout_add (100, (GSourceFunc) webrtcbin_get_stats, webrtc1);

  gst_print ("Starting pipeline\n");
  ret = gst_element_set_state (GST_ELEMENT (pipe1), GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE)
    goto err;

  return TRUE;

err:
  if (pipe1)
    g_clear_object (&pipe1);
  if (webrtc1)
    webrtc1 = NULL;
  return FALSE;
}

static gboolean
register_with_server (void)
{
  gst_print ("-------- register_with_server\n");
  if (soup_websocket_connection_get_state (ws_conn) !=
      SOUP_WEBSOCKET_STATE_OPEN)
    return FALSE;

  app_state = SERVER_REGISTERING;
  return TRUE;
}

static void
on_server_closed (SoupWebsocketConnection * conn G_GNUC_UNUSED,
    gpointer user_data G_GNUC_UNUSED)
{
  gst_print ("-------- on_server_closed\n");
  app_state = SERVER_CLOSED;
  cleanup_and_quit_loop ("Server connection closed", 0);
}


static void
on_server_message (SoupWebsocketConnection * conn, SoupWebsocketDataType type,
    GBytes * message, gpointer user_data)
{
  gst_print ("-------- on_server_message\n");
  gchar *text;

  switch (type) {
    case SOUP_WEBSOCKET_DATA_BINARY:
      gst_printerr ("Received unknown binary message, ignoring\n");
      return;
    case SOUP_WEBSOCKET_DATA_TEXT:{
      gsize size;
      const gchar *data = g_bytes_get_data (message, &size);
      text = g_strndup (data, size);
      gst_print ("[WebSocket TEXT Message] Size: %lu, Content: %.*s\n",
          (unsigned long) size, (int) size, data);
      break;
    }
    default:
      g_assert_not_reached ();
  }

  JsonNode *root;
  JsonObject *object, *child;
  JsonParser *parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, text, -1, NULL)) {
    gst_printerr ("Unknown message '%s', ignoring\n", text);
    g_object_unref (parser);
    goto out;
  }

  root = json_parser_get_root (parser);
  if (!JSON_NODE_HOLDS_OBJECT (root)) {
    gst_printerr ("Unknown json message '%s', ignoring\n", text);
    g_object_unref (parser);
    goto out;
  }

  object = json_node_get_object (root);

  if (!json_object_has_member (object, "type")) {
    gst_printerr ("No 'type' field in message: '%s'\n", text);
    g_object_unref (parser);
    goto out;
  }

  gint type_value = json_object_get_int_member (object, "type");

  if (!ws1Id && json_object_has_member (object, "ws1Id")) {
    ws1Id = g_strdup (json_object_get_string_member (object, "ws1Id"));
  }

  if (!ws2Id && json_object_has_member (object, "ws2Id")) {
    ws2Id = g_strdup (json_object_get_string_member (object, "ws2Id"));
  }

  switch (type_value) {
    case SENDER_SESSION_ID_ISSUANCE:{
      if (!ws1Id && json_object_has_member (object, "sessionId")) {
        ws1Id = g_strdup (json_object_get_string_member (object, "sessionId"));
      }
      break;
    }
    case SENDER_MEDIA_DEVICE_LIST_REQUEST:{
      gst_print ("MEDIA_DEVICE_LIST_REQUEST from %s\n", ws2Id);

      JsonObject *payload = json_object_new ();
      json_object_set_string_member (payload, "source", "gstreamer");

      json_object_set_array_member (payload, "devices", get_media_devices ());
      json_object_set_object_member (payload, "codecs",
          get_supported_codecs ());

      ws_send (ws_conn, RECEIVER_MEDIA_DEVICE_LIST_RESPONSE, ws1Id, ws2Id,
          payload);
      break;
    }
    case SENDER_MEDIA_STREAM_START:{
      if (!start_pipeline (RTP_OPUS_DEFAULT_PT, RTP_VP8_DEFAULT_PT)) {
        cleanup_and_quit_loop ("ERROR: failed to start pipeline",
            PEER_CALL_ERROR);
      }

      break;
    }
    case SENDER_SDP_ANSWER:{
      JsonObject *answer_obj = json_object_get_object_member (object, "answer");
      const gchar *sdp_str = json_object_get_string_member (answer_obj, "sdp");
      gst_print ("SDP_ANSWER: %s\n", sdp_str);

      GstSDPMessage *sdp;
      GstWebRTCSessionDescription *desc;
      GstPromise *promise;

      if (gst_sdp_message_new (&sdp) != GST_SDP_OK) {
        gst_printerr ("Failed to create SDP message\n");
        break;
      }

      if (gst_sdp_message_parse_buffer ((guint8 *) sdp_str, strlen (sdp_str),
              sdp) != GST_SDP_OK) {
        gst_printerr ("Failed to parse SDP message\n");
        gst_sdp_message_free (sdp);
        break;
      }

      desc =
          gst_webrtc_session_description_new (GST_WEBRTC_SDP_TYPE_ANSWER, sdp);
      promise = gst_promise_new ();

      g_signal_emit_by_name (webrtc1, "set-remote-description", desc, promise);
      gst_promise_interrupt (promise);
      gst_promise_unref (promise);
      gst_webrtc_session_description_free (desc);
      break;
    }

    case SENDER_ICE:{
      JsonObject *candidate_obj =
          json_object_get_object_member (object, "candidate");
      const gchar *candidate_str =
          json_object_get_string_member (candidate_obj, "candidate");
      gint sdpMLineIndex =
          json_object_get_int_member (candidate_obj, "sdpMLineIndex");
      g_signal_emit_by_name (webrtc1, "add-ice-candidate", sdpMLineIndex,
          candidate_str);
      break;
    }
    case SENDER_SYSTEM_ERROR:
    default:
      gst_print ("Unhandled message type: %d\n", type_value);
      break;
  }
  g_object_unref (parser);

out:
  g_free (text);
}

static void
on_server_connected (SoupSession * session, GAsyncResult * res,
    SoupMessage * msg)
{
  gst_print ("-------- on_server_connected\n");

  GError *error = NULL;

  ws_conn = soup_session_websocket_connect_finish (session, res, &error);
  if (error) {
    cleanup_and_quit_loop (error->message, SERVER_CONNECTION_ERROR);
    g_error_free (error);
    return;
  }

  g_assert_nonnull (ws_conn);

  app_state = SERVER_CONNECTED;
  gst_print ("Connected to signalling server\n");

  g_signal_connect (ws_conn, "closed", G_CALLBACK (on_server_closed), NULL);
  g_signal_connect (ws_conn, "message", G_CALLBACK (on_server_message), NULL);

  register_with_server ();
}

static void
connect_to_websocket_server_async (void)
{
  gst_print ("-------- connect_to_websocket_server_async\n");
  SoupLogger *logger;
  SoupMessage *message;
  SoupSession *session;

  session = soup_session_new_with_options (NULL);

#if SOUP_CHECK_VERSION(3,0,0)
  logger = soup_logger_new (SOUP_LOGGER_LOG_BODY);
#else
  logger = soup_logger_new (SOUP_LOGGER_LOG_BODY, -1);
#endif
  soup_session_add_feature (session, SOUP_SESSION_FEATURE (logger));
  g_object_unref (logger);

  message = soup_message_new (SOUP_METHOD_GET, server_url);

  gst_print ("Connecting to server...\n");

  /* Once connected, we will register */
  const char *protocols[] = { "sender", NULL };
  soup_session_websocket_connect_async (session, message, NULL, protocols,
#if SOUP_CHECK_VERSION(3,0,0)
      G_PRIORITY_DEFAULT,
#endif
      NULL, (GAsyncReadyCallback) on_server_connected, message);
  app_state = SERVER_CONNECTING;
}

int
gst_main (int argc, char *argv[])
{
  GOptionContext *context;
  GError *error = NULL;
  int ret_code = -1;

  context = g_option_context_new ("- gstreamer webrtc sendonly demo");

  g_option_context_add_group (context, gst_init_get_option_group ());
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    gst_printerr ("Error initializing: %s\n", error->message);
    return -1;
  }

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "webrtc-sendonly", 0,
      "WebRTC Sending and Receiving example");

  if (!check_plugins ()) {
    goto out;
  }

  ret_code = 0;

  loop = g_main_loop_new (NULL, FALSE);

  connect_to_websocket_server_async ();

  g_main_loop_run (loop);

  if (loop)
    g_main_loop_unref (loop);

  if (pipe1) {
    GstBus *bus;

    gst_element_set_state (GST_ELEMENT (pipe1), GST_STATE_NULL);
    gst_print ("Pipeline stopped\n");

    bus = gst_pipeline_get_bus (GST_PIPELINE (pipe1));
    gst_bus_remove_watch (bus);
    gst_object_unref (bus);

    gst_object_unref (pipe1);
  }

out:
  // g_free (sender_id);
  // g_free (our_id);

  return ret_code;
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
