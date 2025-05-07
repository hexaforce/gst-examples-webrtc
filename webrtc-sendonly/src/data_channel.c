#include "headers/data_channel.h"
#include "headers/common.h"

static void
on_data_channel_message (GObject * dc, gchar * str, gpointer user_data)
{
  g_print ("Received message: %s\n", str);
}

void
setup_data_channel (GObject * data_channel)
{
  g_signal_connect (data_channel, "on-message-string",
      G_CALLBACK (on_data_channel_message), NULL);
}

void
on_data_channel (GstElement * webrtc, GObject * data_channel,
    gpointer user_data)
{
  g_print ("New data channel created\n");
  setup_data_channel (data_channel);
}
