CC=gcc
LIBS=-lcanna -ltermcap
CFLAGS=-O2 -g -Wall -I/usr/local/canna/include
LDFLAGS=-L/usr/local/canna/lib

#CC=gcc
#LIBS=-lcanna -lncurses
#CFLAGS=-O2 -g -Wall -I/usr/local/canna/include -I/usr/local/include/ncurses -I/usr/local/include
#LDFLAGS=-L/usr/local/canna/lib -L/usr/local/lib

TARGET=pefnac
OBJS=pefnac.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -rf $(TARGET) $(OBJS)

pefnac.o: Makefile pefnac.c

install:
	install -s $(TARGET) /usr/local/bin/
