#include "headers/common.h"
#include "headers/pipeline.h"

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

  g_print ("Received message: %.*s\n", (int) size, data);
  g_free (text);
}

static void
on_server_connected (SoupSession * session, GAsyncResult * res,
    gpointer user_data)
{
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
}

void
connect_to_websocket_server_async ()
{
  SoupSession *session = soup_session_new ();
  SoupMessage *msg = soup_message_new (SOUP_METHOD_GET, "ws://localhost:8443");
  char *protocols[] = { (char *) "webrtc", NULL };

  soup_session_websocket_connect_async (session,
      msg,
      NULL,
      protocols,
      G_PRIORITY_DEFAULT,
      NULL, (GAsyncReadyCallback) on_server_connected, NULL);

  app_state = SERVER_CONNECTING;
}
