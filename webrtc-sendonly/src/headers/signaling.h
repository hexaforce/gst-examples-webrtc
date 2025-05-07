#ifndef SIGNALING_H
#define SIGNALING_H

#include "common.h"

void connect_to_websocket_server_async (void);
void on_server_message (SoupWebsocketConnection * conn,
    SoupWebsocketDataType type, GBytes * message, gpointer user_data);

#endif
