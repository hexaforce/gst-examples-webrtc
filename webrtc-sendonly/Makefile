.PHONY: all fmt clean

all: clean vtx

CC = gcc
CFLAGS = `pkg-config --cflags gstreamer-1.0 gstreamer-webrtc-1.0 libsoup-3.0 json-glib-1.0` -Iheaders
LIBS = `pkg-config --libs gstreamer-1.0 gstreamer-webrtc-1.0 libsoup-3.0 json-glib-1.0`

SRC = src/main.c src/signaling.c src/pipeline.c src/webrtc.c src/data_channel.c src/utils.c
OBJ = $(SRC:.c=.o)
HEADERS = $(wildcard src/headers/*.h)

vtx: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LIBS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

fmt:
	find . \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 gst-indent-1.0

clean:
	rm -f $(OBJ) vtx
	find . \( -name '*~' \) -delete
