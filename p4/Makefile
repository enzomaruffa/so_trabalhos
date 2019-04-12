#Dumbest makefile ever

main: ppos_core.c ppos_data.h ppos.h queue.c queue.h pingpong-scheduler.c
	gcc -o test ppos_core.c pingpong-scheduler.c queue.c -g -Wall
