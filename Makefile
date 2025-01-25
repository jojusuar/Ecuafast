CC = gcc
CFLAGS = -g
LFLAGS = -lm

TARGET1 = sri
TARGET2 = ecuafast
TARGET3 = senae
TARGET4 = supercia
TARGET5 = boatgenerator
TARGET6 = portadmin
TARGETS = $(TARGET1) $(TARGET2) $(TARGET3) $(TARGET4) $(TARGET5) $(TARGET6)

SRCS1 = sri.c common.c
SRCS2 = ecuafast.c
SRCS3 = senae.c common.c heap.c
SRCS4 = supercia.c common.c
SRCS5 = boatgenerator.c
SRCS6 = portadmin.c common.c list.c

OBJS1 = $(SRCS1:.c=.o)
OBJS2 = $(SRCS2:.c=.o)
OBJS3 = $(SRCS3:.c=.o)
OBJS4 = $(SRCS4:.c=.o)
OBJS5 = $(SRCS5:.c=.o)
OBJS6 = $(SRCS6:.c=.o)
OBJS = $(OBJS1) $(OBJS2) $(OBJS3) $(OBJS4) $(OBJS5) $(OBJS6)

all: $(TARGETS)

$(TARGET1): $(OBJS1) 
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

$(TARGET2): $(OBJS2)
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

$(TARGET3): $(OBJS3)
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

$(TARGET4): $(OBJS4)
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

$(TARGET5): $(OBJS5)
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

$(TARGET6): $(OBJS6)
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

%.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGETS) *.o

.PHONY: all clean
