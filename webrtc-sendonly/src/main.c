#include "headers/common.h"
#include "headers/signaling.h"

GMainLoop *loop = NULL;
GstElement *pipe1 = NULL, *webrtc1 = NULL;
SoupWebsocketConnection *ws_conn = NULL;
AppState app_state = APP_STATE_UNKNOWN;
gchar *ws1Id = NULL, *ws2Id = NULL;
const gchar *server_url = "ws://localhost:8443/signaling";
GObject *send_channel = NULL;
int
main (int argc, char *argv[])
{
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  connect_to_websocket_server_async ();

  g_main_loop_run (loop);

  if (loop)
    g_main_loop_unref (loop);
  if (pipe1)
    gst_object_unref (pipe1);
  g_free (ws1Id);
  g_free (ws2Id);

  return 0;
}
