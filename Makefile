CC=gcc
LIBS=-lcanna -ltermcap
CFLAGS=-O2 -g -Wall -I/usr/local/canna/include -DUNIX98
LDFLAGS=-L/usr/local/canna/lib

#CC=gcc
#LIBS=-lcanna -lncurses
#CFLAGS=-O2 -g -Wall -I/usr/local/canna/include -I/usr/local/include/ncurses -I/usr/local/include
#LDFLAGS=-L/usr/local/canna/lib -L/usr/local/lib

TARGET=pefnac
OBJS=pefnac.o

TARGET-UTF8=pefnac-utf8
OBJS-UTF8=pefnac-utf8.o

all: $(TARGET) $(TARGET-UTF8)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

$(TARGET-UTF8): $(OBJS-UTF8)
	$(CC) $(CFLAGS) -o $@ $(OBJS-UTF8) $(LDFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -rf $(TARGET) $(OBJS) $(TARGET-UTF8) $(OBJS-UTF8)

pefnac.o: Makefile pefnac.c

pefnac-utf8.o: Makefile pefnac-utf8.c

install:
	install -s $(TARGET) /usr/local/bin/
	install -s $(TARGET-UTF8) /usr/local/bin/
