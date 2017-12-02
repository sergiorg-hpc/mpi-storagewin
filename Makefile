
CC       = mpicc
MPICC    = mpicc
AR       = ar
INCDIR   = -I./ -I./benchmark -I/usr/include/mpi
LIBDIR   = -L./
MPI_SWIN = -lmpi_swin
CFLAGS   = $(INCDIR) $(LIBDIR)

CC_CHECK  := $(shell which CC 2> /dev/null)
LFS_CHECK := $(shell which lfs 2> /dev/null)

ifdef CC_CHECK
    CC    = CC
    MPICC = CC
endif

ifdef USE_PROFILE
    MPI_SWIN = -profile=mpi_swin
endif

#ifdef LUSTRE_PATH
ifdef LFS_CHECK
#    INCDIR           += -I$(LUSTRE_PATH)/include
#    LIBDIR           += -L$(LUSTRE_PATH)/lib64
#    CFLAGS           += -llustreapi
    DMPI_SWIN_LUSTRE  = -DMPI_SWIN_LUSTRE=1
endif

all: mpi_swin_test.out mpi_swin_test_dynamic.out mstream.out

mstream.out:  libmpi_swin.a
	@$(MPICC) $(CFLAGS) $(MPI_SWIN) benchmark/mstream.c \
									-o benchmark/mstream.out

mpi_swin_test.out:  libmpi_swin.a
	@$(MPICC) $(CFLAGS) $(MPI_SWIN) mpi_swin_test.c -o mpi_swin_test.out

mpi_swin_test_dynamic.out:  libmpi_swin.a
	@$(MPICC) $(CFLAGS) $(MPI_SWIN) mpi_swin_test_dynamic.c \
									-o mpi_swin_test_dynamic.out

libmpi_swin.a: mpiwrappers.o mpiwrappers_util.o mfile.o
	@$(AR) -cq libmpi_swin.a mpiwrappers*.o mfile.o
	
mpiwrappers.o:
	@$(CC) $(CFLAGS) $(DMPI_SWIN_LUSTRE) -c mpiwrappers.c
	
mpiwrappers_util.o:
	@$(CC) $(CFLAGS) -c mpiwrappers_util.c
	
mfile.o:
	@$(CC) $(CFLAGS) -c mfile.c

clean: 
	@$(RM) *.out benchmark/*.out *.o *.a *~ *.tmp *.win

rebuild: clean all

