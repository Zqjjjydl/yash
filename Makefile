yash: yash.o parse.o job.o
	gcc yash.o parse.o job.o -o yash -lreadline

yash.o: yash.c
	gcc -c -g yash.c -o yash.o

parse.o: parse.c parse.h command.h
	gcc -c -g parse.c -o parse.o

job.o: job.c job.h
	gcc -c -g job.c -o job.o

clean:
	rm yash.o parse.o job.o
	#  ./-lreadline