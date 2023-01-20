yash: yash.o parse.o
	gcc yash.o parse.o -o yash -lreadline

yash.o: yash.c
	gcc -c -g yash.c -o yash.o

parse.o: parse.c parse.h command.h
	gcc -c -g parse.c -o parse.o

clean:
	rm yash.o parse.o
	#  ./-lreadline