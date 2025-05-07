#ifndef UTILS_H
#define UTILS_H

#include <glib.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <gst/gst.h>

#define SENDER_SESSION_ID_ISSUANCE        0
#define SENDER_MEDIA_DEVICE_LIST_REQUEST  1
#define SENDER_MEDIA_STREAM_START         2
#define SENDER_SDP_ANSWER                 3
#define SENDER_ICE                        4
#define SENDER_SYSTEM_ERROR               9

#define RECEIVER_SESSION_ID_ISSUANCE           0
#define RECEIVER_CHANGE_SENDER_ENTRIES         1
#define RECEIVER_MEDIA_DEVICE_LIST_RESPONSE    2
#define RECEIVER_SDP_OFFER                     3
#define RECEIVER_ICE                           4
#define RECEIVER_SYSTEM_ERROR                  9

gboolean check_plugins (void);

void ws_send (SoupWebsocketConnection * conn, int type, const gchar * ws1Id,
    const gchar * ws2Id, JsonObject * data);

JsonArray *get_media_devices (void);

JsonObject *get_supported_codecs (void);

gchar *get_string_from_json_object (JsonObject * object);

JsonObject *get_json_object_from_string (SoupWebsocketDataType type,
    GBytes * message);

#endif // UTILS_H
