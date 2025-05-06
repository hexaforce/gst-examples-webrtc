.PHONY: all clean fmt

all: clean webrtc-unidirectional-h264 webrtc-recvonly-h264 webrtc-sendrecv

CC     := gcc
LIBS   := $(shell pkg-config --libs --cflags glib-2.0 gstreamer-1.0 gstreamer-rtp-1.0 gstreamer-sdp-1.0 gstreamer-webrtc-1.0 json-glib-1.0 libsoup-3.0 gstreamer-webrtc-nice-1.0)
CFLAGS := -O0 -ggdb -Wall -fno-omit-frame-pointer \
		$(shell pkg-config --cflags glib-2.0 gstreamer-1.0 gstreamer-rtp-1.0 gstreamer-sdp-1.0 gstreamer-webrtc-1.0 json-glib-1.0 libsoup-3.0)

webrtc-unidirectional-h264: webrtc-unidirectional-h264.c
	"$(CC)" $(CFLAGS) $^ $(LIBS) -o $@

webrtc-recvonly-h264: webrtc-recvonly-h264.c
	"$(CC)" $(CFLAGS) $^ $(LIBS) -o $@

webrtc-sendrecv: webrtc-sendrecv.c custom_agent.c
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

clean:
	rm -f webrtc-unidirectional-h264 *.o
	rm -rf webrtc-unidirectional-h264.dSYM
	rm -f webrtc-recvonly-h264 *.o
	rm -rf webrtc-recvonly-h264.dSYM
	rm -f webrtc-sendrecv *.o
	rm -rf webrtc-sendrecv.dSYM
	rm -f *~

fmt:
	find . \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 gst-indent-1.0
