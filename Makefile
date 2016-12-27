CC=gcc
DEBUG=yes
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -g -O2
LDFLAGS=-lpthread -lrt -lnuma
OBJ=matrix_naif.c
EXEC=matrix


all : $(EXEC)

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
