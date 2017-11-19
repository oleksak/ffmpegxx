
TARGET := read_rtsp

ifdef DEBUG
TARGET := $(TARGET)d
endif

SOURCES := \
	./read_rtsp.cpp

DEPENDENCIES := \
	./ffmpegxx.hpp

CPPFLAGS := -std=c++14

ifdef DEBUG
CPPFLAGS += -D_DEBUG -Og -ggdb -Wall -g
endif

CPPFLAGS += `pkg-config --cflags libavcodec libavformat libswscale libavutil`
LIBS += `pkg-config --libs libavcodec libavformat libswscale libavutil`

.PHONY: all
all: 	$(TARGET)

$(TARGET): $(SOURCES) $(DEPENDENCIES)
	g++ $(SOURCES) $(CPPFLAGS) $(INCLUDES) $(LIBS) -o $(TARGET)

clean:
	rm -f ./$(TARGET)

prepare:
	apt-get install -y libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
