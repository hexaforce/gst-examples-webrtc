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

SoupWebsocketConnection *receiver;
SoupWebsocketConnection *transceiver;

void
soup_websocket_message_cb2 (G_GNUC_UNUSED SoupWebsocketConnection * connection,
    SoupWebsocketDataType data_type, GBytes * message, gpointer user_data)
{
  gsize size;
  const gchar *data;
  gchar *data_string;
  const gchar *type_string;
  const gchar *sdp_string;
  JsonNode *root_json;
  JsonObject *root_json_object;
  JsonObject *data_json_object;
  JsonParser *json_parser = NULL;

  switch (data_type) {
    case SOUP_WEBSOCKET_DATA_BINARY:
      g_error ("Received unknown binary message, ignoring\n");
      return;

    case SOUP_WEBSOCKET_DATA_TEXT:
      data = g_bytes_get_data (message, &size);
      /* Convert to NULL-terminated string */
      data_string = g_strndup (data, size);
      break;

    default:
      g_assert_not_reached ();
  }

  json_parser = json_parser_new ();
  if (!json_parser_load_from_data (json_parser, data_string, -1, NULL))
    goto unknown_message;

  root_json = json_parser_get_root (json_parser);
  if (!JSON_NODE_HOLDS_OBJECT (root_json))
    goto unknown_message;

  root_json_object = json_node_get_object (root_json);

  if (json_object_has_member (root_json_object, "type")) {
    type_string = json_object_get_string_member (root_json_object, "type");
    if (json_object_has_member (root_json_object, "sdp")) {
      GstSDPMessage *sdp;
      int ret;
      sdp_string = json_object_get_string_member (root_json_object, "sdp");
      gst_print ("Received SDP:\n%s\n", sdp_string);
      ret = gst_sdp_message_new (&sdp);
      g_assert_cmphex (ret, ==, GST_SDP_OK);
    }
  } else {
    if (!json_object_has_member (root_json_object, "candidate")) {
      goto unknown_message;
    }
    gst_print ("Received ICE:\n%s\n", data_string);
  }

  const char *protocol = soup_websocket_connection_get_protocol (connection);

  if (protocol) {
    if (g_strcmp0 (protocol, "receiver") == 0) {
      if (transceiver) {
        soup_websocket_connection_send_text (transceiver, data_string);
      }
    } else if (g_strcmp0 (protocol, "transceiver") == 0) {
      if (receiver) {
        soup_websocket_connection_send_text (receiver, data_string);
      }
    }
  }

cleanup:
  if (json_parser != NULL)
    g_object_unref (G_OBJECT (json_parser));
  g_free (data_string);
  return;

unknown_message:
  g_error ("Unknown message \"%s\", ignoring", data_string);
  goto cleanup;
}

void
soup_websocket_handler (G_GNUC_UNUSED SoupServer * server,
    SoupWebsocketConnection * connection, G_GNUC_UNUSED const char *path,
    G_GNUC_UNUSED SoupClientContext * client_context, gpointer user_data)
{
  GHashTable *receiver_entry_table = (GHashTable *) user_data;
  const char *protocol = soup_websocket_connection_get_protocol (connection);

  gst_print ("protocol %s\n", protocol);

  g_signal_connect (G_OBJECT (connection), "closed",
      G_CALLBACK (soup_websocket_closed_cb), (gpointer) receiver_entry_table);

  if (protocol) {
    if (g_strcmp0 (protocol, "receiver") == 0) {
      receiver = connection;
      gst_print ("Processing new websocket connection %p\n",
          (gpointer) receiver);
    } else if (g_strcmp0 (protocol, "transceiver") == 0) {
      transceiver = connection;
      gst_print ("Processing new websocket connection %p\n",
          (gpointer) transceiver);
    }
  }
  g_object_ref (G_OBJECT (connection));

  g_signal_connect (G_OBJECT (connection), "message",
      G_CALLBACK (soup_websocket_message_cb2), NULL);

  g_hash_table_replace (receiver_entry_table, connection, connection);
  return;

  // cleanup:
  //   destroy_receiver_entry((gpointer)receiver_entry);
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
  gst_debug_set_default_threshold (GST_LEVEL_WARNING);

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
  char *protocols[] = { "receiver", "transceiver", NULL };
  soup_server_add_websocket_handler (soup_server, "/ws", NULL, protocols,
      soup_websocket_handler, (gpointer) receiver_entry_table, NULL);
  soup_server_listen_all (soup_server, SOUP_HTTP_PORT,
      (SoupServerListenOptions) 0, NULL);

  gst_print ("WebRTC page link: http://127.0.0.1:%d/\n", (gint) SOUP_HTTP_PORT);
  gst_print ("WebRTC page link: http://127.0.0.1:%d/receiver.html\n",
      (gint) SOUP_HTTP_PORT);
  gst_print ("WebRTC page link: http://127.0.0.1:%d/transceiver.html\n",
      (gint) SOUP_HTTP_PORT);

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
