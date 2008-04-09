all: check_mix

check_mix: check_mix.o
	gcc -o check_mix check_mix.o /usr/lib/libexpat.a
	
check_mix.o: check_mix.c
	gcc -c -pedantic -Wall check_mix.c
		
clean:
	rm -f *.o
	rm -f check_mix