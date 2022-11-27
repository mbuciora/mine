CCFLAGS = -lncurses -pthread -lm -w

main: main.c
	gcc  main.c $(CCFLAGS) -o main 

clean:
	rm -rf *.o main

