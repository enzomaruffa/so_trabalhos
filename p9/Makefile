#Dumbest makefile ever

task = pingpong-sleep.c

main: ppos_core.c ppos_data.h ppos.h queue.c queue.h $(task)
	gcc -o test ppos_core.c $(task) queue.c -g -Wall

debug: ppos_core.c ppos_data.h ppos.h queue.c queue.h $(task)
	gcc -o test ppos_core.c $(task) queue.c -g -Wall -DDEBUG