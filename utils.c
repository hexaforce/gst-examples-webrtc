#include "utils.h"

gboolean
check_plugins (void)
{
  int i;
  gboolean ret;
  GstPlugin *plugin;
  GstRegistry *registry;
  const gchar *needed[] = { "opus", "vpx", "nice", "webrtc", "dtls", "srtp",
    "rtpmanager", "videotestsrc", "audiotestsrc", NULL
  };

  registry = gst_registry_get ();
  ret = TRUE;
  for (i = 0; i < g_strv_length ((gchar **) needed); i++) {
    plugin = gst_registry_find_plugin (registry, needed[i]);
    if (!plugin) {
      gst_print ("Required gstreamer plugin '%s' not found\n", needed[i]);
      ret = FALSE;
      continue;
    }
    gst_object_unref (plugin);
  }
  return ret;
}

void
ws_send (SoupWebsocketConnection * conn, int type, const gchar * ws1Id,
    const gchar * ws2Id, JsonObject * data)
{
  JsonObject *msg = json_object_new ();

  json_object_set_int_member (msg, "type", type);
  json_object_set_string_member (msg, "ws1Id", ws1Id);
  json_object_set_string_member (msg, "ws2Id", ws2Id);

  GList *members = json_object_get_members (data);
  for (GList * l = members; l != NULL; l = l->next) {
    const gchar *key = l->data;
    JsonNode *value = json_object_get_member (data, key);
    json_object_set_member (msg, key, json_node_copy (value));
  }
  g_list_free (members);

  JsonNode *root = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (root, msg);

  JsonGenerator *gen = json_generator_new ();
  json_generator_set_root (gen, root);
  gchar *json_str = json_generator_to_data (gen, NULL);

  soup_websocket_connection_send_text (conn, json_str);

  g_object_unref (gen);
  json_node_free (root);
  g_free (json_str);
}

JsonArray *
get_media_devices ()
{
  GstDeviceMonitor *monitor;
  GList *devices, *l;
  JsonArray *device_array = json_array_new ();

  gst_init (NULL, NULL);
  monitor = gst_device_monitor_new ();

  gst_device_monitor_add_filter (monitor, "Video/Source", NULL);
  gst_device_monitor_add_filter (monitor, "Audio/Source", NULL);

  gst_device_monitor_start (monitor);

  devices = gst_device_monitor_get_devices (monitor);
  int index = 0;

  for (l = devices; l != NULL; l = l->next, index++) {
    GstDevice *device = GST_DEVICE (l->data);
    const gchar *name = gst_device_get_display_name (device);
    const gchar *klass = gst_device_get_device_class (device);
    GstCaps *caps = gst_device_get_caps (device);
    gchar *caps_str = caps ? gst_caps_to_string (caps) : g_strdup ("none");

    JsonObject *dev = json_object_new ();

    gchar *index_str = g_strdup_printf ("%d", index);
    json_object_set_string_member (dev, "id", index_str);
    json_object_set_string_member (dev, "name", name);
    json_object_set_string_member (dev, "klass", klass ? klass : "unknown");
    json_object_set_string_member (dev, "caps", caps_str);

    json_array_add_object_element (device_array, dev);

    g_free (index_str);
    g_free (caps_str);
    if (caps)
      gst_caps_unref (caps);
  }

  g_list_free_full (devices, (GDestroyNotify) gst_object_unref);
  gst_device_monitor_stop (monitor);
  g_object_unref (monitor);

  return device_array;
}

static gchar *
normalize_mime (const gchar * gst_mime)
{
  if (g_str_has_prefix (gst_mime, "video/x-vp8"))
    return g_strdup ("video/VP8");
  if (g_str_has_prefix (gst_mime, "video/x-vp9"))
    return g_strdup ("video/VP9");
  if (g_str_has_prefix (gst_mime, "video/x-h264"))
    return g_strdup ("video/H264");
  if (g_str_has_prefix (gst_mime, "video/x-h265"))
    return g_strdup ("video/H26");
  if (g_str_has_prefix (gst_mime, "video/x-av1"))
    return g_strdup ("video/AV1");
  if (g_str_has_prefix (gst_mime, "audio/x-opus"))
    return g_strdup ("audio/opus");
  if (g_str_has_prefix (gst_mime, "audio/x-mulaw"))
    return g_strdup ("audio/PCMU");
  if (g_str_has_prefix (gst_mime, "audio/x-alaw"))
    return g_strdup ("audio/PCMA");
  if (g_str_has_prefix (gst_mime, "audio/G722"))
    return g_strdup ("audio/G722");
  return NULL;
}

static void
copy_structure_to_json (const GstStructure * s, JsonObject * entry)
{
  int n_fields = gst_structure_n_fields (s);
  for (int i = 0; i < n_fields; i++) {
    const gchar *key = gst_structure_nth_field_name (s, i);
    const GValue *val = gst_structure_get_value (s, key);

    if (G_VALUE_HOLDS_INT (val)) {
      json_object_set_int_member (entry, key, g_value_get_int (val));
    } else if (G_VALUE_HOLDS_UINT (val)) {
      json_object_set_int_member (entry, key, g_value_get_uint (val));
    } else if (G_VALUE_HOLDS_DOUBLE (val)) {
      json_object_set_double_member (entry, key, g_value_get_double (val));
    } else if (G_VALUE_HOLDS_STRING (val)) {
      json_object_set_string_member (entry, key, g_value_get_string (val));
    } else if (G_VALUE_HOLDS_BOOLEAN (val)) {
      json_object_set_boolean_member (entry, key, g_value_get_boolean (val));
    } else {

    }
  }
}

JsonObject *
get_supported_codecs ()
{
  JsonObject *root = json_object_new ();
  JsonArray *video_codecs = json_array_new ();
  JsonArray *audio_codecs = json_array_new ();

  GList *factories =
      gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_ENCODER,
      GST_RANK_NONE);

  for (GList * l = factories; l != NULL; l = l->next) {
    GstElementFactory *factory = (GstElementFactory *) l->data;
    const GList *templates =
        gst_element_factory_get_static_pad_templates (factory);
    for (const GList * t = templates; t != NULL; t = t->next) {
      GstStaticPadTemplate *tpl = (GstStaticPadTemplate *) t->data;
      if (tpl->direction != GST_PAD_SRC)
        continue;

      GstCaps *caps = gst_static_pad_template_get_caps (tpl);
      guint caps_size = gst_caps_get_size (caps);
      for (guint i = 0; i < caps_size; i++) {
        GstStructure *s = gst_caps_get_structure (caps, i);
        const gchar *raw_mime = gst_structure_get_name (s);
        gchar *mime = normalize_mime (raw_mime);
        if (!mime)
          continue;

        JsonObject *entry = json_object_new ();
        json_object_set_string_member (entry, "mimeType", mime);

        copy_structure_to_json (s, entry);

        if (g_str_has_prefix (mime, "video/"))
          json_array_add_object_element (video_codecs, entry);
        else
          json_array_add_object_element (audio_codecs, entry);

        g_free (mime);
      }
      gst_caps_unref (caps);
    }
  }

  g_list_free_full (factories, gst_object_unref);

  json_object_set_array_member (root, "video", video_codecs);
  json_object_set_array_member (root, "audio", audio_codecs);
  return root;
}

gchar *
get_string_from_json_object (JsonObject * object)
{
  JsonNode *root;
  JsonGenerator *generator;
  gchar *text;

  /* Make it the root node */
  root = json_node_init_object (json_node_alloc (), object);
  generator = json_generator_new ();
  json_generator_set_root (generator, root);
  text = json_generator_to_data (generator, NULL);

  /* Release everything */
  g_object_unref (generator);
  json_node_free (root);
  return text;
}

JsonObject *
get_json_object_from_string (SoupWebsocketDataType type, GBytes * message)
{
  gchar *text = NULL;
  JsonObject *object = NULL;

  if (type == SOUP_WEBSOCKET_DATA_BINARY) {
    gst_printerr ("Received unknown binary message, ignoring\n");
    return NULL;
  }

  if (type == SOUP_WEBSOCKET_DATA_TEXT) {
    gsize size;
    const gchar *data = g_bytes_get_data (message, &size);
    text = g_strndup (data, size);
  } else {
    g_assert_not_reached ();
  }

  JsonParser *parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, text, -1, NULL)) {
    gst_printerr ("Unknown message '%s', ignoring\n", text);
    g_object_unref (parser);
    g_free (text);
    return NULL;
  }

  JsonNode *root = json_parser_get_root (parser);
  if (!JSON_NODE_HOLDS_OBJECT (root)) {
    gst_printerr ("Unknown json message '%s', ignoring\n", text);
    g_object_unref (parser);
    g_free (text);
    return NULL;
  }

  object = json_node_get_object (root);

  /* Check type of JSON message */
  if (!json_object_has_member (object, "type")) {
    gst_printerr ("No 'type' field in message: '%s'\n", text);
    g_object_unref (parser);
    g_free (text);
    return NULL;
  }

  g_object_unref (parser);
  g_free (text);

  return object;
}
