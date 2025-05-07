#include "headers/data_channel.h"
#include "headers/common.h"

static void on_data_channel_message(GObject *dc, gchar *str, gpointer user_data) {
  g_print("Received message: %s\n", str);
}

void setup_data_channel(GObject *data_channel) {
  g_signal_connect(data_channel, "on-message-string", G_CALLBACK(on_data_channel_message), NULL);
}

void on_data_channel(GstElement *webrtc, GObject *data_channel, gpointer user_data) {
  g_print("New data channel created\n");
  setup_data_channel(data_channel);
}

void connect_data_channel_signals(GObject *dc) {
  g_signal_connect(dc, "on-error", G_CALLBACK(cleanup_and_quit_loop), NULL);
  g_signal_connect(dc, "on-open", G_CALLBACK(g_print), "DataChannel opened\n");
  g_signal_connect(dc, "on-close", G_CALLBACK(cleanup_and_quit_loop), NULL);
  g_signal_connect(dc, "on-message-string", G_CALLBACK(g_print), NULL);
}