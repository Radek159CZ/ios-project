cc = gcc
cflags = -std=gnu99 -Wall -Wextra -Werror -pedantic
ldlibs = -lrt -lpthread

# vysledny binarni soubor
bin = proj2
# objektivni soubory
objs = main.o utilities.o

all: $(bin)

$(bin): $(objs)
	$(cc) $(cflags) $(objs) -o $(bin) $(ldlibs)

main.o: main.c utilities.h
	$(cc) $(cflags) -c main.c -o main.o

utilities.o: utilities.c utilities.h
	$(cc) $(cflags) -c utilities.c -o utilities.o

clean:
	rm -f *.o $(bin) proj2.out