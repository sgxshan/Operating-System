all : ext2_ls ext2_cp ext2_mkdir ext2_ln ext2_rm read

ext2_ls : ext2_ls.o util.o
	gcc -Wall -Werror -g -o ext2_ls ext2_ls.o util.o

ext2_cp : ext2_cp.o util.o
	gcc -Wall -Werror -g -o ext2_cp ext2_cp.o util.o

ext2_mkdir : ext2_mkdir.o util.o
	gcc -Wall -Werror -g -o ext2_mkdir ext2_mkdir.o util.o

ext2_ln : ext2_ln.o util.o
	gcc -Wall -Werror -g -o ext2_ln ext2_ln.o util.o

ext2_rm : ext2_rm.o util.o
	gcc -Wall -Werror -g -o ext2_rm ext2_rm.o util.o

read : readimage.o util.o
	gcc -Wall -Werror -g -o read readimage.o util.o

%.o : %.c
	gcc -Wall -g -c $<

clean:
	rm -f ext2_ls ext2_cp ext2_mkdir ext2_ln ext2_rm read *.o *~
