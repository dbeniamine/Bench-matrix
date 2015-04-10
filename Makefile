CC=gcc
DEBUG=yes
FLAGS=-std=gnu99 -Wall -Wextra -Werror
ifeq ($(DEBUG),yes)
	CFLAGS=$(FLAGS) -g #-static
else
	CFLAGS=$(FLAGS) -O2
endif
LDFLAGS=-lpthread -lrt -lnuma
OBJ=matrix_naif.c
EXEC=matrix


all : $(EXEC).x

$(EXEC) : $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# For kmaf
$(EXEC).x : $(EXEC)
	cp $(EXEC) $(EXEC).x

.PHONY : clean mrproper

clean :
	rm -rf *.o

distclean : clean
	rm -rf $(EXEC) $(EXEC).x
