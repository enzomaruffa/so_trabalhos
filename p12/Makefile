#Dumbest makefile ever

task = pingpong-mqueue.c

main: ppos_core.c ppos_data.h ppos.h queue.c queue.h $(task)
	gcc -o test ppos_core.c $(task) queue.c -g -Wall -D_XOPEN_SOURCE=600 -lm

debug: ppos_core.c ppos_data.h ppos.h queue.c queue.h $(task)
	gcc -o test ppos_core.c $(task) queue.c -g -Wall -DDEBUG -D_XOPEN_SOURCE=600 -lm