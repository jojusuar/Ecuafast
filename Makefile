CC = gcc
CFLAGS = -g
LFLAGS = -lm
TARGET = sri
SRCS = sri.c common.c map.c list.c
OBJS = $(SRCS:.c=.o)
HEADERS = boat.h common.h map.h list.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LFLAGS)
	$(CC) -g -o ecuafast ecuafast.c

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) sri ecuafast

.PHONY: all clean