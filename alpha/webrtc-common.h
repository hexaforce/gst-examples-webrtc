#ifndef __WEBRTC_COMMON_H__
#define __WEBRTC_COMMON_H__

#include <glib.h>
#include <gst/gst.h>
#include <gst/sdp/sdp.h>
#include <locale.h>

#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>

#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <string.h>

G_BEGIN_DECLS typedef struct _ReceiverEntry ReceiverEntry;

struct _ReceiverEntry
{
  SoupWebsocketConnection *connection;

  GstElement *pipeline;
  GstElement *webrtcbin;
};

gchar *read_file (const gchar * filename);

gchar *get_string_from_json_object (JsonObject * object);

gboolean bus_watch_cb (GstBus * bus, GstMessage * message, gpointer user_data);

void soup_websocket_message_cb (G_GNUC_UNUSED SoupWebsocketConnection *
    connection, SoupWebsocketDataType data_type, GBytes * message,
    gpointer user_data);

void on_negotiation_needed_cb (GstElement * webrtcbin, gpointer user_data);

void on_ice_candidate_cb (G_GNUC_UNUSED GstElement * webrtcbin,
    guint mline_index, gchar * candidate, gpointer user_data);

void on_ice_gathering_state_notify (GstElement * webrtcbin, GParamSpec * pspec,
    gpointer user_data);

void destroy_receiver_entry (gpointer receiver_entry_ptr);

void soup_websocket_closed_cb (SoupWebsocketConnection * connection,
    gpointer user_data);

void soup_http_handler (G_GNUC_UNUSED SoupServer * soup_server,
    SoupMessage * message, const char *path, G_GNUC_UNUSED GHashTable * query,
    G_GNUC_UNUSED SoupClientContext * client_context,
    G_GNUC_UNUSED gpointer user_data);

void connect_data_channel_signals (GObject * data_channel);

gboolean webrtcbin_get_stats (GstElement * webrtcbin);

G_END_DECLS
#endif /* __WEBRTC_COMMON_H__ */
