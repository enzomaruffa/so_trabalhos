#Dumbest makefile ever

main: ppos_core.c ppos_data.h ppos.h queue.c queue.h pingpong-maintask.c
	gcc -o test ppos_core.c pingpong-maintask.c queue.c -g -Wall

debug: ppos_core.c ppos_data.h ppos.h queue.c queue.h pingpong-maintask.c
	gcc -o test ppos_core.c pingpong-maintask.c queue.c -g -Wall -DDEBUG