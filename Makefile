CC := gcc

SOURCE := Program.cpp
TARGET := wwfc-dns-proxy
CFLAGS := -x c++ -O3 -Wall -Wextra -Werror -std=c++11
LDFLAGS := -lstdc++ -static

ifeq ($(OS),Windows_NT)
	TARGET := $(TARGET).exe
	CFLAGS += -DWIN32
	LDFLAGS += -lws2_32
endif

all: $(TARGET)

clean:
	rm -f $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) -o $(TARGET) $(CFLAGS) $(SOURCE) $(LDFLAGS)
