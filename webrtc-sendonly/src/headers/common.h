#ifndef COMMON_H
#define COMMON_H
#define GST_USE_UNSTABLE_API

#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

typedef enum
{
  APP_STATE_UNKNOWN = 0,
  APP_STATE_ERROR = 1,
  SERVER_CONNECTING = 1000,
  SERVER_CONNECTION_ERROR,
  SERVER_CONNECTED,
  SERVER_REGISTERING = 2000,
  SERVER_REGISTRATION_ERROR,
  SERVER_REGISTERED,
  SERVER_CLOSED,
  PEER_CONNECTING = 3000,
  PEER_CONNECTION_ERROR,
  PEER_CONNECTED,
  PEER_CALL_NEGOTIATING = 4000,
  PEER_CALL_STARTED,
  PEER_CALL_STOPPING,
  PEER_CALL_STOPPED,
  PEER_CALL_ERROR
} AppState;

extern GMainLoop *loop;
extern GstElement *pipe1, *webrtc1;
extern SoupWebsocketConnection *ws_conn;
extern AppState app_state;
extern gchar *ws1Id, *ws2Id;

#define RTP_OPUS_DEFAULT_PT 97
#define RTP_VP8_DEFAULT_PT 96
#define STUN_SERVER "stun://stun.l.google.com:19302"

gboolean cleanup_and_quit_loop (const gchar * msg, AppState state);

#endif
