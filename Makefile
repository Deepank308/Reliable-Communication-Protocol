make:
	gcc -Wall -c rsocket.c -o rsocket.o
	ar rcs librsocket.a rsocket.o
	rm rsocket.o
	###### insert your .c file here ######
	gcc -Wall user1.c -o 1 -L. librsocket.a
	gcc -Wall user2.c -o 2 -L. librsocket.a
clean:
	rm -f *.a 1 2
