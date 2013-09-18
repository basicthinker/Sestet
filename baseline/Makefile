CC = arm-none-linux-gnueabi-gcc # to cross-compile
CFLAGS += -Wall # -g

all : baseline-bench baseline-simu

baseline-bench : baseline-bench.c
	$(CC) $(CFLAGS) -static -march=armv7-a -o $@.o $^

baseline-simu : baseline-simu.c
	$(CC) $(CFLAGS) -static -march=armv7-a -o $@.o $^

clean :
	rm -rf *.o
