CC	:= gcc
ifeq ($(shell uname), Darwin)
  LIBS := $(shell PKG_CONFIG_PATH=/usr/local/opt/libsoup@2/lib/pkgconfig pkg-config --libs --cflags glib-2.0 gstreamer-1.0 gstreamer-rtp-1.0 gstreamer-sdp-1.0 gstreamer-webrtc-1.0 json-glib-1.0 libsoup-2.4 gstreamer-webrtc-nice-1.0)
else
  LIBS := $(shell pkg-config --libs --cflags glib-2.0 gstreamer-1.0 gstreamer-rtp-1.0 gstreamer-sdp-1.0 gstreamer-webrtc-1.0 json-glib-1.0 libsoup-2.4 gstreamer-webrtc-nice-1.0)
endif
CFLAGS	:= -O0 -ggdb -Wall -fno-omit-frame-pointer

all: clean webrtc-unidirectional-h264 webrtc-recvonly-h264 webrtc-sendrecv webrtc-soup

webrtc-unidirectional-h264: webrtc-unidirectional-h264.c webrtc-common.c
	"$(CC)" $(CFLAGS) $^ $(LIBS) -o $@

webrtc-recvonly-h264: webrtc-recvonly-h264.c webrtc-common.c
	"$(CC)" $(CFLAGS) $^ $(LIBS) -o $@

webrtc-sendrecv: webrtc-sendrecv.c custom_agent.c
	"$(CC)" $(CFLAGS) $^ $(LIBS) -o $@

webrtc-soup: webrtc-soup.c webrtc-common.c
	"$(CC)" $(CFLAGS) $^ $(LIBS) -o $@

clean:
	rm -f webrtc-unidirectional-h264
	rm -rf webrtc-unidirectional-h264.dSYM
	rm -f webrtc-recvonly-h264
	rm -rf webrtc-recvonly-h264.dSYM
	rm -f webrtc-sendrecv
	rm -rf webrtc-sendrecv.dSYM
	rm -f webrtc-soup
	rm -rf webrtc-soup.dSYM
	rm -f *~

fmt:
	find . \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 gst-indent-1.0
