
// static void
// on_incoming_stream (GstElement * webrtc, GstPad * pad, GstElement * pipe)
// {
//   GstElement *decodebin;
//   GstPad *sinkpad;

//   if (GST_PAD_DIRECTION (pad) != GST_PAD_SRC)
//     return;

//   decodebin = gst_element_factory_make ("decodebin", NULL);
//   g_signal_connect (decodebin, "pad-added",
//       G_CALLBACK (on_incoming_decodebin_stream), pipe);
//   gst_bin_add (GST_BIN (pipe), decodebin);
//   gst_element_sync_state_with_parent (decodebin);

//   sinkpad = gst_element_get_static_pad (decodebin, "sink");
//   gst_pad_link (pad, sinkpad);
//   gst_object_unref (sinkpad);
// }

// /* Answer created by our pipeline, to be sent to the peer */
// static void
// on_answer_created (GstPromise * promise, gpointer user_data)
// {
//   GstWebRTCSessionDescription *answer = NULL;
//   const GstStructure *reply;

//   g_assert_cmphex (app_state, ==, PEER_CALL_NEGOTIATING);

//   g_assert_cmphex (gst_promise_wait (promise), ==, GST_PROMISE_RESULT_REPLIED);
//   reply = gst_promise_get_reply (promise);
//   gst_structure_get (reply, "answer",
//       GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL);
//   gst_promise_unref (promise);

//   promise = gst_promise_new ();
//   g_signal_emit_by_name (webrtc1, "set-local-description", answer, promise);
//   gst_promise_interrupt (promise);
//   gst_promise_unref (promise);

//   /* Send answer to peer */
//   send_sdp_to_peer (answer);
//   gst_webrtc_session_description_free (answer);
// }

// static void
// on_offer_set (GstPromise * promise, gpointer user_data)
// {
//   gst_promise_unref (promise);
//   promise = gst_promise_new_with_change_func (on_answer_created, NULL, NULL);
//   g_signal_emit_by_name (webrtc1, "create-answer", NULL, promise);
// }

// static void
// on_offer_received (GstSDPMessage * sdp)
// {
//   GstWebRTCSessionDescription *offer = NULL;
//   GstPromise *promise;

//   /* If we got an offer and we have no webrtcbin, we need to parse the SDP,
//    * get the payload types, then start the pipeline */
//   if (!webrtc1 && our_id) {
//     guint medias_len, formats_len;
//     guint opus_pt = 0, vp8_pt = 0;

//     gst_println ("Parsing offer to find payload types");

//     medias_len = gst_sdp_message_medias_len (sdp);
//     for (int i = 0; i < medias_len; i++) {
//       const GstSDPMedia *media = gst_sdp_message_get_media (sdp, i);
//       formats_len = gst_sdp_media_formats_len (media);
//       for (int j = 0; j < formats_len; j++) {
//         guint pt;
//         GstCaps *caps;
//         GstStructure *s;
//         const char *fmt, *encoding_name;

//         fmt = gst_sdp_media_get_format (media, j);
//         if (g_strcmp0 (fmt, "webrtc-datachannel") == 0)
//           continue;
//         pt = atoi (fmt);
//         caps = gst_sdp_media_get_caps_from_media (media, pt);
//         s = gst_caps_get_structure (caps, 0);
//         encoding_name = gst_structure_get_string (s, "encoding-name");
//         if (vp8_pt == 0 && g_strcmp0 (encoding_name, "VP8") == 0)
//           vp8_pt = pt;
//         if (opus_pt == 0 && g_strcmp0 (encoding_name, "OPUS") == 0)
//           opus_pt = pt;
//       }
//     }

//     g_assert_cmpint (opus_pt, !=, 0);
//     g_assert_cmpint (vp8_pt, !=, 0);

//     gst_println ("Starting pipeline with opus pt: %u vp8 pt: %u", opus_pt,
//         vp8_pt);

//     if (!start_pipeline (FALSE, opus_pt, vp8_pt)) {
//       cleanup_and_quit_loop ("ERROR: failed to start pipeline",
//           PEER_CALL_ERROR);
//     }
//   }

//   offer = gst_webrtc_session_description_new (GST_WEBRTC_SDP_TYPE_OFFER, sdp);
//   g_assert_nonnull (offer);

//   /* Set remote description on our pipeline */
//   {
//     promise = gst_promise_new_with_change_func (on_offer_set, NULL, NULL);
//     g_signal_emit_by_name (webrtc1, "set-remote-description", offer, promise);
//   }
//   gst_webrtc_session_description_free (offer);
// }

  // if (json_object_has_member (object, "sdp")) {
  //   int ret;
  //   GstSDPMessage *sdp;
  //   const gchar *text, *sdptype;
  //   GstWebRTCSessionDescription *answer;

  //   app_state = PEER_CALL_NEGOTIATING;

  //   child = json_object_get_object_member (object, "sdp");

  //   if (!json_object_has_member (child, "type")) {
  //     cleanup_and_quit_loop ("ERROR: received SDP without 'type'",
  //         PEER_CALL_ERROR);
  //     goto out;
  //   }

  //   sdptype = json_object_get_string_member (child, "type");
  //   /* In this example, we create the offer and receive one answer by default,
  //    * but it's possible to comment out the offer creation and wait for an offer
  //    * instead, so we handle either here.
  //    *
  //    * See tests/examples/webrtcbidirectional.c in gst-plugins-bad for another
  //    * example how to handle offers from peers and reply with answers using webrtcbin. */
  //   text = json_object_get_string_member (child, "sdp");
  //   ret = gst_sdp_message_new (&sdp);
  //   g_assert_cmphex (ret, ==, GST_SDP_OK);
  //   ret = gst_sdp_message_parse_buffer ((guint8 *) text, strlen (text), sdp);
  //   g_assert_cmphex (ret, ==, GST_SDP_OK);

  //   if (g_str_equal (sdptype, "answer")) {
  //     gst_print ("Received answer:\n%s\n", text);
  //     answer = gst_webrtc_session_description_new (GST_WEBRTC_SDP_TYPE_ANSWER,
  //         sdp);
  //     g_assert_nonnull (answer);

  //     /* Set remote description on our pipeline */
  //     {
  //       GstPromise *promise = gst_promise_new ();
  //       g_signal_emit_by_name (webrtc1, "set-remote-description", answer,
  //           promise);
  //       gst_promise_interrupt (promise);
  //       gst_promise_unref (promise);
  //     }
  //     app_state = PEER_CALL_STARTED;
  //   } else {
  //     gst_print ("Received offer:\n%s\n", text);
  //     on_offer_received (sdp);
  //   }

  // } else if (json_object_has_member (object, "ice")) {
  //   const gchar *candidate;
  //   gint sdpmlineindex;

  //   child = json_object_get_object_member (object, "ice");
  //   candidate = json_object_get_string_member (child, "candidate");
  //   sdpmlineindex = json_object_get_int_member (child, "sdpMLineIndex");

  //   /* Add ice candidate sent by remote peer */
  //   g_signal_emit_by_name (webrtc1, "add-ice-candidate", sdpmlineindex,
  //       candidate);
  // } else {
  //   gst_printerr ("Ignoring unknown JSON message:\n%s\n", text);
  // }
