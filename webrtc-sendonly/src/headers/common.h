#ifndef COMMON_H
#define COMMON_H

#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>

typedef enum AppState
{
  APP_STATE_UNKNOWN = 0,
  APP_STATE_ERROR = 1,
  SERVER_CONNECTING = 1000,
  SERVER_CONNECTION_ERROR,
  SERVER_CONNECTED,
  SERVER_REGISTERING,
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
  PEER_CALL_ERROR,
} AppState;

extern GObject *send_channel;

// WebRTC signaling message types
#define SENDER_MEDIA_STREAM_START 100
#define SENDER_SDP_ANSWER         101
#define SENDER_ICE                102
#define RECEIVER_SDP_OFFER        201
#define RECEIVER_ICE              202

#define RTP_OPUS_DEFAULT_PT 97
#define RTP_VP8_DEFAULT_PT  96

// External globals
extern GMainLoop *loop;
extern GstElement *pipe1;
extern GstElement *webrtc1;
extern SoupWebsocketConnection *ws_conn;
extern AppState app_state;
extern gchar *ws1Id;
extern gchar *ws2Id;
extern const gchar *server_url;

// Declarations
gboolean cleanup_and_quit_loop (const gchar * msg, AppState state);

void send_ice_candidate (GstElement * webrtc, guint mlineindex,
    gchar * candidate, gpointer user_data);

void send_sdp_offer (GstWebRTCSessionDescription * desc);

void ws_send (SoupWebsocketConnection * conn, int type, const gchar * ws1Id,
    const gchar * ws2Id, JsonObject * data);

#endif
