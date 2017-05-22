
CC      = gcc
MPICC   = mpicc
AR      = ar
INCDIR  = -I./ -I/usr/include/mpich
LIBDIR  = -L./
PGASIO  = -profile=pgasio
CFLAGS  = $(INCDIR) $(LIBDIR)

all: pgasio_test.out pgasio_test_dynamic.out

beskow: CC      = CC
beskow: MPICC   = CC
beskow: PGASIO  = -lpgasio
beskow: all

tegner: CC      = mpicc
tegner: all

pgasio_test.out:  libpgasio.a pgasio_test.o
	@$(MPICC) $(CFLAGS) $(PGASIO) -o pgasio_test.out pgasio_test.o

pgasio_test_dynamic.out:  libpgasio.a pgasio_test_dynamic.o
	@$(MPICC) $(CFLAGS) $(PGASIO) -o pgasio_test_dynamic.out pgasio_test_dynamic.o

pgasio_test.o:  pgasio_test.c mfile.h mpi_swin_keys.h common.h
	@$(CC) $(CFLAGS) -c pgasio_test.c
	
pgasio_test_dynamic.o:  pgasio_test_dynamic.c mfile.h mpi_swin_keys.h common.h
	@$(CC) $(CFLAGS) -c pgasio_test_dynamic.c
	
libpgasio.a: mpiwrappers.o mpiwrappers_util.o mfile.o
	@$(AR) -cq libpgasio.a mpiwrappers.o mpiwrappers_util.o mfile.o
	
mpiwrappers.o:  mpiwrappers.c mpiwrappers.h mfile.h common.h
	@$(CC) $(CFLAGS) -c mpiwrappers.c
	
mpiwrappers_util.o:  mpiwrappers_util.c mpiwrappers_util.h common.h
	@$(CC) $(CFLAGS) -c mpiwrappers_util.c
	
mfile.o:  mfile.c mfile.h common.h
	@$(CC) $(CFLAGS) -c mfile.c

clean: 
	@$(RM) *.out *.o *.a *~ *.tmp

rebuild: clean all

rebuild_beskow: clean beskow

rebuild_tegner: clean tegner

