#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <mpi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRUE               1
#define FALSE              0
#define ERROR             -1
#define PATH_MAX        4096
#define DEBUG_PRINT        0

#define CHK(_hr) \
{ \
    int hr = _hr; \
    if(hr != MPI_SUCCESS) \
    { \
        errno = (errno == MPI_SUCCESS) ? EPERM : errno; \
        return hr; \
    } \
}

#define CHKPRINT(_hr) \
{ \
    int hr = _hr; \
    if(hr != MPI_SUCCESS) \
    { \
        fprintf(stderr, "Error in %s:%d (%d / %s)\n", __FILE__, __LINE__, hr, strerror(errno)); \
        return hr; \
    } \
}

#define CHKB(b) { if(b) { CHK(ERROR); } }

#if DEBUG_PRINT
    #define PREFIX "[P%d] %s:%d -> %s() / "
    #define SUFFIX "\n"
    
    #define DBGPRINTF(input, ...) \
    { \
        int rank = 0; \
        MPI_Comm_rank(MPI_COMM_WORLD, &rank); \
        if (DEBUG_PRINT < 0 || (rank + 1) == DEBUG_PRINT) \
            fprintf(stderr, PREFIX input SUFFIX, rank, __FILE__, __LINE__, __func__, __VA_ARGS__); \
    }
    
    #define DBGPRINT(input) \
    { \
        int rank = 0; \
        MPI_Comm_rank(MPI_COMM_WORLD, &rank); \
        if (DEBUG_PRINT < 0 || (rank + 1) == DEBUG_PRINT) \
            fprintf(stderr, PREFIX input SUFFIX, rank, __FILE__, __LINE__, __func__); \
    }
#else
    #define DBGPRINTF(input, ...) { }
    #define DBGPRINT(input)       { }
#endif

#ifdef __cplusplus
}
#endif

