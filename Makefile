CC = gcc
CFLAGS = -g
LFLAGS = -lm
TARGET = core
SRCS = core.c common.c map.c list.c
OBJS = $(SRCS:.c=.o)
HEADERS = core.h common.h map.h list.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LFLAGS)
	$(CC) -g -o client client.c
	$(CC) -g -o test_traffic test_traffic.c

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) test_traffic client

.PHONY: all clean