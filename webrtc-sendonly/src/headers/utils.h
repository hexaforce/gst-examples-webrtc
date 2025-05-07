#ifndef UTILS_H
#define UTILS_H

#include "common.h"

gboolean cleanup_and_quit_loop (const gchar * msg, AppState state);
JsonArray *get_media_devices (void);
JsonObject *get_supported_codecs (void);
gboolean check_plugins (void);

#endif
