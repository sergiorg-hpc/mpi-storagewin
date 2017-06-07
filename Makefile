
CC       = mpicc
MPICC    = mpicc
AR       = ar
INCDIR   = -I./ -I/usr/include/mpi
LIBDIR   = -L./
MPI_SWIN = -lmpi_swin
CFLAGS   = $(INCDIR) $(LIBDIR)

CC_CHECK      := $(shell which CC 2> /dev/null)
PROFILE_CHECK := $(shell man mpicc 2>&1 | tee 2>&1 | grep profile= 2> /dev/null)
LFS_CHECK     := $(shell which lfs 2> /dev/null)

ifdef CC_CHECK
    CC    = CC
    MPICC = CC
endif

ifdef PROFILE_CHECK
    MPI_SWIN = -profile=mpi_swin
endif

#ifdef LUSTRE_PATH
ifdef LFS_CHECK
#    INCDIR           += -I$(LUSTRE_PATH)/include
#    LIBDIR           += -L$(LUSTRE_PATH)/lib64
#    CFLAGS           += -llustreapi
    DMPI_SWIN_LUSTRE  = -DMPI_SWIN_LUSTRE=1
endif

all: libmpi_swin.a mpi_swin_test.out mpi_swin_test_dynamic.out

mpi_swin_test.out:  libmpi_swin.a mpi_swin_test.o
	@$(MPICC) $(CFLAGS) $(MPI_SWIN) -o mpi_swin_test.out mpi_swin_test.o

mpi_swin_test_dynamic.out:  libmpi_swin.a mpi_swin_test_dynamic.o
	@$(MPICC) $(CFLAGS) $(MPI_SWIN) -o mpi_swin_test_dynamic.out mpi_swin_test_dynamic.o

mpi_swin_test.o:  mpi_swin_test.c mfile.h mpi_swin_keys.h common.h
	@$(CC) $(CFLAGS) -c mpi_swin_test.c
	
mpi_swin_test_dynamic.o:  mpi_swin_test_dynamic.c mfile.h mpi_swin_keys.h common.h
	@$(CC) $(CFLAGS) -c mpi_swin_test_dynamic.c
	
libmpi_swin.a: mpiwrappers.o mpiwrappers_util.o mfile.o
	@$(AR) -cq libmpi_swin.a mpiwrappers*.o mfile.o
	
mpiwrappers.o:  mpiwrappers.c mpiwrappers.h mfile.h common.h
	@$(CC) $(CFLAGS) $(DMPI_SWIN_LUSTRE) -c mpiwrappers.c
	
mpiwrappers_util.o:  mpiwrappers_util.c mpiwrappers_util.h common.h
	@$(CC) $(CFLAGS) -c mpiwrappers_util.c
	
mfile.o:  mfile.c mfile.h common.h
	@$(CC) $(CFLAGS) -c mfile.c

clean: 
	@$(RM) *.out *.o *.a *~ *.tmp *.win

rebuild: clean all

