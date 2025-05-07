#include "headers/common.h"
#include "headers/pipeline.h"
#include "headers/utils.h"
#include "headers/webrtc.h"

gboolean
register_with_server (void)
{
  g_print ("Pretending to register with server (placeholder)\n");
  return TRUE;
}

static void
on_server_closed (SoupWebsocketConnection * conn, gpointer user_data)
{
  app_state = SERVER_CLOSED;
  cleanup_and_quit_loop ("Server connection closed", 0);
}

void
on_server_message (SoupWebsocketConnection * conn,
    SoupWebsocketDataType type, GBytes * message, gpointer user_data)
{
  gsize size;
  const gchar *data = g_bytes_get_data (message, &size);
  gchar *text = g_strndup (data, size);

  JsonParser *parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, text, -1, NULL)) {
    g_printerr ("Failed to parse message: %s\n", text);
    g_object_unref (parser);
    g_free (text);
    return;
  }

  JsonNode *root = json_parser_get_root (parser);
  if (!JSON_NODE_HOLDS_OBJECT (root)) {
    g_printerr ("Message is not a JSON object\n");
    g_object_unref (parser);
    g_free (text);
    return;
  }

  JsonObject *object = json_node_get_object (root);
  gint type_value = json_object_get_int_member (object, "type");

  if (!ws1Id && json_object_has_member (object, "ws1Id"))
    ws1Id = g_strdup (json_object_get_string_member (object, "ws1Id"));
  if (!ws2Id && json_object_has_member (object, "ws2Id"))
    ws2Id = g_strdup (json_object_get_string_member (object, "ws2Id"));

  switch (type_value) {
    case SENDER_MEDIA_STREAM_START:{
      if (!start_pipeline (RTP_OPUS_DEFAULT_PT, RTP_VP8_DEFAULT_PT)) {
        cleanup_and_quit_loop ("Failed to start pipeline", PEER_CALL_ERROR);
      }
      break;
    }
    case SENDER_SDP_ANSWER:{
      JsonObject *answer_obj = json_object_get_object_member (object, "answer");
      const gchar *sdp_str = json_object_get_string_member (answer_obj, "sdp");

      GstSDPMessage *sdp;
      if (gst_sdp_message_new (&sdp) != GST_SDP_OK)
        break;
      if (gst_sdp_message_parse_buffer ((guint8 *) sdp_str, strlen (sdp_str),
              sdp) != GST_SDP_OK) {
        gst_sdp_message_free (sdp);
        break;
      }

      GstWebRTCSessionDescription *desc =
          gst_webrtc_session_description_new (GST_WEBRTC_SDP_TYPE_ANSWER, sdp);
      GstPromise *promise = gst_promise_new ();
      g_signal_emit_by_name (webrtc1, "set-remote-description", desc, promise);
      gst_promise_interrupt (promise);
      gst_promise_unref (promise);
      gst_webrtc_session_description_free (desc);
      break;
    }
    case SENDER_ICE:{
      JsonObject *cand = json_object_get_object_member (object, "candidate");
      const gchar *cand_str = json_object_get_string_member (cand, "candidate");
      gint mline = json_object_get_int_member (cand, "sdpMLineIndex");
      g_signal_emit_by_name (webrtc1, "add-ice-candidate", mline, cand_str);
      break;
    }
    default:
      g_print ("Unhandled message type: %d\n", type_value);
  }

  g_object_unref (parser);
  g_free (text);
}

static void
on_server_connected (GObject * source_object, GAsyncResult * res,
    gpointer user_data)
{
  SoupSession *session = SOUP_SESSION (source_object);
  GError *error = NULL;
  ws_conn = soup_session_websocket_connect_finish (session, res, &error);

  if (error) {
    cleanup_and_quit_loop (error->message, SERVER_CONNECTION_ERROR);
    g_error_free (error);
    return;
  }

  app_state = SERVER_CONNECTED;
  g_signal_connect (ws_conn, "closed", G_CALLBACK (on_server_closed), NULL);
  g_signal_connect (ws_conn, "message", G_CALLBACK (on_server_message), NULL);

  register_with_server ();
}

void
connect_to_websocket_server_async (void)
{
  SoupMessage *msg;
  SoupSession *session = soup_session_new ();

  msg = soup_message_new (SOUP_METHOD_GET, server_url);
  char *protocols[] = { "sender", NULL };

  soup_session_websocket_connect_async (session, msg, NULL, protocols,
#if SOUP_CHECK_VERSION(3, 0, 0)
      G_PRIORITY_DEFAULT,
#endif
      NULL, on_server_connected, NULL);

  app_state = SERVER_CONNECTING;
}
