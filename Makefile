CC = gcc
CFLAGS = -g
LFLAGS = -lm

TARGET1 = sri
TARGET2 = ecuafast
TARGET3 = senae
TARGETS = $(TARGET1) $(TARGET2) $(TARGET3)

SRCS1 = sri.c common.c
SRCS2 = ecuafast.c
SRCS3 = senae.c common.c heap.c

OBJS1 = $(SRCS1:.c=.o)
OBJS2 = $(SRCS2:.c=.o)
OBJS3 = $(SRCS3:.c=.o)
OBJS = $(OBJS1) $(OBJS2) $(OBJS3)

HEADERS1 = boat.h common.h
HEADERS2 = boat.h
HEADERS3 = boat.h common.h heap.h

all: $(TARGETS)

$(TARGET1): $(OBJS1)
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

$(TARGET2): $(OBJS2)
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

$(TARGET3): $(OBJS3)
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGETS) *.o

.PHONY: all clean
