.PHONY: all clean fmt

all: clean main

CC     := gcc
LIBS   := $(shell pkg-config --libs --cflags glib-2.0 gstreamer-1.0 gstreamer-rtp-1.0 gstreamer-sdp-1.0 gstreamer-webrtc-1.0 json-glib-1.0 libsoup-3.0 gstreamer-webrtc-nice-1.0)
CFLAGS := -O0 -ggdb -Wall -fno-omit-frame-pointer \
		$(shell pkg-config --cflags glib-2.0 gstreamer-1.0 gstreamer-rtp-1.0 gstreamer-sdp-1.0 gstreamer-webrtc-1.0 json-glib-1.0 libsoup-3.0)
CFLAGS += -DGST_USE_UNSTABLE_API

main: main.c utils.c custom_agent.c
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

clean:
	rm -f main *.o
	rm -rf main.dSYM
	rm -f *~

fmt:
	find . \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 gst-indent-1.0
