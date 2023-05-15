CC = gcc
FLAGS = -Wall -g 

default: stnc

stnc: stnc.c 
	$(CC) $(FLAGS) stnc.c -o stnc

.PHONY: clean default

clean:
	-rm -f *.o 

