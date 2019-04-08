# TinyFugue Makefile

#FLAGS = -DHARDCODE -DMAILDELAY=6
FLAGS = -DTERMCAP -DSYSVTTY -O2
# CONNECT = -DCONNECT_BSD

### If your version of Make does not import the environment, define HOME here.
# HOME=

### Single-user default file locations and settings

EXE       = tf
TFCONNECT = tf.con
TFLIBRARY = tf.lib
HELPFILE  = tf.hlp

## If you plan to let others use your copy of tf, use the umask 22 line.
# UMASK   = umask 22;
UMASK     =

# The next 2 macros may be left blank, if you don't want the man page.
# MANTYPE is either "nroff" or "cat" (vt100).
MANTYPE   = 
MANPAGE   = 

### GNU C
CC = gcc
GCCFLAGS = -ansi

LIBRARIES = -ltermcap -lsocket
MAKE = make

#################################################################
#### You should not need to modify anything below this point ####
#################################################################

FILES      = -DTFLIBRARY=\"$(TFLIBRARY)\" -DTFCONNECT=\"$(TFCONNECT)\"
CFLAGS     = $(FLAGS) $(GCCFLAGS) $(CCFLAGS) $(FILES) $(CONNECT)
MAKEFILE   = Makefile
SOURCE     = main.c world.c util.c socket.c keyboard.c macro.c \
             command1.c command2.c special.c history.c process.c output.c \
             expand.c dstring.c help.c signal.c fibsboard.c
OBJECTS    = main.o world.o util.o socket.o keyboard.o macro.o \
             command1.o command2.o special.o history.o process.o output.o \
             expand.o dstring.o help.o signal.o fibsboard.o
CONSOURCE  = tf.connect.c


all:
	@echo 'Please read the INSTALLATION file before compiling.'

tf:	$(OBJECTS)
	$(CC) $(CFLAGS) -o tf.exe $(OBJECTS) $(LIBRARIES)

install:	tf
	@echo
	@echo '**** TinyFugue installation complete.'

uninstall:
	rm -f $(EXE) $(TFCONNECT) $(TFLIBRARY) $(MANPAGE)
	rm -f $(HELPFILE) $(HELPFILE).index

clean:
	rm -f $(OBJECTS) makehelp


command1.o: command1.c tf.h dstring.h util.h history.h world.h socket.h \
            output.h macro.h help.h process.h keyboard.h \
            prototype.h $(MAKEFILE)

command2.o: command2.c tf.h dstring.h util.h macro.h keyboard.h output.h \
            command1.h history.h world.h socket.h \
            prototype.h $(MAKEFILE)

dstring.o: dstring.c tf.h dstring.h util.h \
            prototype.h $(MAKEFILE)

expand.o: expand.c tf.h dstring.h util.h macro.h socket.h command1.h output.h \
            prototype.h $(MAKEFILE)

help.o: help.c tf.h dstring.h util.h output.h \
            prototype.h $(MAKEFILE)

history.o: history.c tf.h dstring.h util.h history.h world.h socket.h macro.h \
           output.h \
            prototype.h $(MAKEFILE)

keyboard.o: keyboard.c tf.h dstring.h util.h macro.h output.h history.h \
           socket.h expand.h \
            prototype.h $(MAKEFILE)

macro.o: macro.c tf.h dstring.h util.h history.h world.h socket.h macro.h \
           keyboard.h output.h expand.h command1.h \
            prototype.h $(MAKEFILE)

main.o: main.c tf.h dstring.h util.h history.h world.h socket.h macro.h \
           output.h signal.h keyboard.h command1.h command2.h \
            prototype.h $(MAKEFILE)

output.o: output.c tf.h dstring.h util.h history.h world.h socket.h macro.h \
           output.h \
            prototype.h $(MAKEFILE)

process.o: process.c tf.h dstring.h util.h history.h world.h socket.h macro.h \
           expand.h output.h \
            prototype.h $(MAKEFILE)

signal.o: signal.c tf.h dstring.h util.h history.h world.h process.h socket.h \
           keyboard.h output.h \
            prototype.h $(MAKEFILE)

socket.o: socket.c tf.h dstring.h util.h history.h world.h socket.h output.h \
           process.h macro.h keyboard.h command1.h command2.h special.h \
           signal.h tf.connect.h opensock.c \
            prototype.h $(MAKEFILE)

special.o: special.c tf.h dstring.h util.h history.h world.h socket.h macro.h \
           output.h \
            prototype.h $(MAKEFILE)

util.o: util.c tf.h dstring.h util.h output.h macro.h socket.h keyboard.h \
            prototype.h $(MAKEFILE)

world.o: world.c tf.h dstring.h util.h history.h world.h output.h macro.h \
           process.h \
            prototype.h $(MAKEFILE)

