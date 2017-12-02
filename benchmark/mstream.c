
#include "common.h"
#include <sys/time.h>
#include <stdint.h>
#include "mpi_swin_keys.h"

#define TMP_FOLDER           "./tmp"
#define NUM_ITERATIONS_INIT  1
#define NUM_ITERATIONS       10
#define NUM_ITERATIONS_TOTAL (NUM_ITERATIONS_INIT + NUM_ITERATIONS)

typedef enum
{
    BENCHMARK_SEQUENTIAL = 0,
    BENCHMARK_PADDING,
    BENCHMARK_PRANDOM,
    BENCHMARK_MIXED
} BenchmarkType;

typedef enum
{
    ALLOC_MEM = 0,
    ALLOC_STORAGE
} AllocType;

/**
 * Helper method that allows to create a directory given its path.
 */
int createDir(const char *path)
{
    struct stat st = { 0 };

    // Check if the directory already exists (i.e., ignoring the request)
    if (stat(path, &st) == -1)
    {
        char mkdir_command[PATH_MAX];
        sprintf(mkdir_command, "mkdir -p %s", path);
        
        // For simplicity, we use the mkdir command
        CHK(system(mkdir_command));
    }
    
    return MPI_SUCCESS;
}

/**
 * Helper method that allows to delete a directory given its path.
 */
int deleteDir(const char *path)
{
    char rm_command[PATH_MAX];
    
    sprintf(rm_command, "rm -rf %s", path);
        
    // For simplicity, we use the rm command
    return system(rm_command);
}

/**
 * Helper method that allows to create an MPI_Info object to enable Storage
 * allocations.
 */
int createStorageInfo(int rank, double factor_orig, MPI_Info* info)
{
    char filename[PATH_MAX];
    char factor[PATH_MAX];
    
    // Create the temp. folder
    CHK(createDir(TMP_FOLDER));
    
    // Define the path according to the rank of the process
    sprintf(filename,  "%s/mpi_swin_%d.win", TMP_FOLDER, rank);
    sprintf(factor,    "%.9lf", factor_orig);

    CHK(MPI_Info_create(info));
    CHK(MPI_Info_set(*info, MPI_SWIN_ALLOC_TYPE,    "storage"));
    // CHK(MPI_Info_set(*info, MPI_SWIN_DEVICEID,      "921"));
    CHK(MPI_Info_set(*info, MPI_SWIN_FILENAME,      filename));
    CHK(MPI_Info_set(*info, MPI_SWIN_OFFSET,        "0"));
    CHK(MPI_Info_set(*info, MPI_SWIN_FACTOR,        factor));
    CHK(MPI_Info_set(*info, MPI_SWIN_UNLINK,        "false"));
    // CHK(MPI_Info_set(*info, MPI_IO_ACCESS_STYLE,    "write_mostly"));
    // CHK(MPI_Info_set(*info, MPI_IO_FILE_PERM,       "S_IRUSR | S_IWUSR"));
    // CHK(MPI_Info_set(*info, MPI_IO_STRIPING_FACTOR, "8"));
    // CHK(MPI_Info_set(*info, MPI_IO_STRIPING_UNIT,   "8388608"));
    
    return MPI_SUCCESS;
}

/**
 * Helper method that returns the elapsed time between two time intervals,
 * measured in seconds.
 */
double getElapsed(struct timeval start, struct timeval stop)
{
    return (double)((stop.tv_sec  - start.tv_sec) * 1000000LL +
                    (stop.tv_usec - start.tv_usec)) / 1000000.0;
}

/**
 * Sequential benchmark that stores chunks of a fixed size consecutively or
 * separated by a given padding and combining read / write operations.
 */
int launchSequentialBenchmark(MPI_Win win, int drank, size_t size,
                              size_t size_b, size_t segment_size,
                              size_t padding)
{
    off_t offset       = 0;
    int   write_active = TRUE;
    int   write_count  = 0;
    char  *baseptr_tmp = (char *)malloc(segment_size);
    
    for (size_t offset_b = 0; offset_b < size_b; offset_b += segment_size)
    {
        if (write_active)
        {
            CHK(MPI_Put(baseptr_tmp, segment_size, MPI_CHAR, drank, offset,
                        segment_size, MPI_CHAR, win));
        }
        else
        {
            CHK(MPI_Get(baseptr_tmp, segment_size, MPI_CHAR, drank, offset,
                        segment_size, MPI_CHAR, win));
        }
        
        CHK(MPI_Win_flush_local(drank, win));
        
        offset       = (offset + padding) % size;
        write_active = !write_active;
    }
    
    free(baseptr_tmp);
    
    return MPI_SUCCESS;
}

/**
 * Random benchmark that stores chunks of a fixed size randomly and
 * combining read / write operations.
 */
int launchRandomBenchmark(MPI_Win win, int drank, size_t size, size_t size_b,
                          size_t segment_size)
{
    off_t    offset       = 0;
    int      write_active = TRUE;
    int      write_count  = 0;
    char     *baseptr_tmp = (char *)malloc(segment_size);
    uint32_t seed         = 921;
    
    // Set the seed
    rand_r(&seed);
    
    for (size_t offset_b = 0; offset_b < size_b; offset_b += segment_size)
    {
        offset = ((size_t)rand_r(&seed) * segment_size) % size;
        
        if (write_active)
        {
            CHK(MPI_Put(baseptr_tmp, segment_size, MPI_CHAR, drank, offset,
                        segment_size, MPI_CHAR, win));
        }
        else
        {
            CHK(MPI_Get(baseptr_tmp, segment_size, MPI_CHAR, drank, offset,
                        segment_size, MPI_CHAR, win));
        }
        
        CHK(MPI_Win_flush_local(drank, win));
        
        write_active = !write_active;
    }
    
    free(baseptr_tmp);
    
    return MPI_SUCCESS;
}

int main (int argc, char *argv[])
{
    BenchmarkType  benchmark    = BENCHMARK_SEQUENTIAL;
    AllocType      alloc_type   = ALLOC_MEM;
    size_t         alloc_size   = 0;
    double         alloc_factor = 0.0;
    size_t         segment_size = 0;
    MPI_Win        win          = MPI_WIN_NULL;
    MPI_Info       info         = MPI_INFO_NULL;
    int            rank         = 0;
    int            num_procs    = 0;
    char           *baseptr     = NULL;
    struct timeval start[2]     = { 0 };
    struct timeval stop[2]      = { 0 };
    
    // Check if the number of parameters match the expected
    if (argc != 6)
    {
        fprintf(stderr, "Error: The number of parameters is incorrect!\n");
        return -1;
    }
    
    // Initialize MPI and retrieve the rank of the process
    CHKPRINT(MPI_Init(&argc, &argv));
    CHKPRINT(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    CHKPRINT(MPI_Comm_size(MPI_COMM_WORLD, &num_procs));
    
    // Retrieve the benchmark settings
    sscanf(argv[1], "%d",  (int *)&benchmark);
    sscanf(argv[2], "%d",  (int *)&alloc_type);
    sscanf(argv[3], "%zu", &alloc_size);
    sscanf(argv[4], "%lf", &alloc_factor);
    sscanf(argv[5], "%zu", &segment_size);
    
    // Define the MPI Info object based on the allocation type
    if (alloc_type == ALLOC_STORAGE)
    {
        CHKPRINT(createStorageInfo(rank, alloc_factor, &info));
    }
    
    // Allocate the window with the specified size
    CHKPRINT(MPI_Win_allocate(alloc_size, sizeof(char), info, MPI_COMM_WORLD,
                              (void**)&baseptr, &win));
    
    // Lock the window in exclusive mode
    CHKPRINT(MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win));
    
    // Launch the benchmark
    for (uint32_t iteration = 0; iteration < NUM_ITERATIONS_TOTAL; iteration++)
    {
        // Start the timer after the initial iterations
        if (iteration == NUM_ITERATIONS_INIT)
        {
            gettimeofday(&start[0], NULL);
        }
        
        if (benchmark == BENCHMARK_SEQUENTIAL)
        {
            CHK(launchSequentialBenchmark(win, rank, alloc_size, alloc_size,
                                          segment_size, segment_size));
        }
        else if (benchmark == BENCHMARK_PADDING)
        {
            CHK(launchSequentialBenchmark(win, rank, alloc_size, alloc_size,
                                          segment_size, (segment_size << 1)));
        }
        else if (benchmark == BENCHMARK_PRANDOM)
        {
            CHK(launchRandomBenchmark(win, rank, alloc_size, alloc_size,
                                      segment_size));
        }
        else if (benchmark == BENCHMARK_MIXED)
        {
            CHK(launchRandomBenchmark(win, rank, alloc_size, (alloc_size >> 1),
                                      segment_size));
            CHK(launchSequentialBenchmark(win, rank, alloc_size,
                                          (alloc_size >> 1), segment_size,
                                          (segment_size << 1)));
        }
    }
    
    // Synchronize the storage window
    gettimeofday(&start[1], NULL);
    if (alloc_type == ALLOC_STORAGE)
    {
        CHKPRINT(MPI_Win_sync(win));
    }
    gettimeofday(&stop[1], NULL);
    gettimeofday(&stop[0], NULL);
    
    // Unlock the window
    CHKPRINT(MPI_Win_unlock(rank, win));
    
    // Print the result
    {
        double elapsed       = getElapsed(start[0], stop[0]);
        double elapsed_flush = getElapsed(start[1], stop[1]);
        double bandwidth     = (alloc_size * NUM_ITERATIONS) / elapsed;
        double bandwidth_mb  = bandwidth / 1048576.0;
        
        printf("%d; %d; %zu; %.9lf; %zu; %.6lf; %.6lf; %.6lf\n", benchmark,
                                                                 alloc_type,
                                                                 alloc_size,
                                                                 alloc_factor,
                                                                 segment_size,
                                                                 elapsed,
                                                                 elapsed_flush,
                                                                 bandwidth_mb);
    }
    
    // Force all processes to wait before releasing
    CHKPRINT(MPI_Barrier(MPI_COMM_WORLD));
    
    // Release the window and finalize the MPI session
    CHKPRINT(MPI_Win_free(&win));
    CHKPRINT(MPI_Finalize());
    
    // Delete the temp. folder
    CHKPRINT(deleteDir(TMP_FOLDER));
    
    return MPI_SUCCESS;
}

