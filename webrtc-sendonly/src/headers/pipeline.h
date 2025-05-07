#ifndef PIPELINE_H
#define PIPELINE_H

#include "common.h"

gboolean start_pipeline (guint opus_pt, guint vp8_pt);

void on_negotiation_needed (GstElement * element, gpointer user_data);

#endif
