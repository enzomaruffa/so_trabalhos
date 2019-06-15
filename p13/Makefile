#Dumbest makefile ever!

task = pingpong-disco.c

main: ppos_core.c ppos_data.h ppos.h queue.c queue.h hard_disk.c hard_disk.h ppos_disk.c ppos_disk.h $(task)
	gcc -o test ppos_core.c $(task) queue.c -g -Wall -D_XOPEN_SOURCE=600 -lm -lrt

debug: ppos_core.c ppos_data.h ppos.h queue.c queue.h hard_disk.c hard_disk.h ppos_disk.c ppos_disk.h $(task)
	gcc -o test ppos_core.c $(task) queue.c -g -Wall -DDEBUG -D_XOPEN_SOURCE=600 -lm -lrt