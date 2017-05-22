
#include "common.h"
#include "mpi_swin_keys.h"

#define IS_ODD_NUM(num) (num & 1)

/**
 * Helper method that allows to create a default MPI_Info object that sets the
 * allocation of the window to the storage device.
 */
int createDefaultInfo(int rank, size_t offset_orig, MPI_Info* info)
{
    char filename[PATH_MAX];
    char offset[PATH_MAX];
    
    // Define the path according to the rank of the process
    sprintf(filename, "./pgasio_mpi_%d.tmp", rank);
    sprintf(offset, "%zu", offset_orig);

    CHK(MPI_Info_create(info));
    CHK(MPI_Info_set(*info, MPI_SWIN_ENABLED,  "true"));
    //CHK(MPI_Info_set(*info, MPI_SWIN_DEVICEID, "921"));
    CHK(MPI_Info_set(*info, MPI_SWIN_FILENAME, filename));
    CHK(MPI_Info_set(*info, MPI_SWIN_OFFSET,   offset));
    CHK(MPI_Info_set(*info, MPI_SWIN_UNLINK,   "false"));
    
    return MPI_SUCCESS;
}

/**
 * Main method that creates an MPI Window for each process and forces the odd
 * ranks to have the allocation in storage. The example will make each even
 * process to write on the odd ones "rank plus the target rank", at the rank
 * position (e.g., process 2 will write "2 + 1" in "offset = 2 * sizeof(int)"
 * within the window of process 1).
 */
int main (int argc, char *argv[])
{
    MPI_Win win;
    MPI_Info info = MPI_INFO_NULL;
    int rank, num_procs;
    int *baseptr;
    
    // Initialize MPI and retrieve the rank of the process
    CHKPRINT(MPI_Init(&argc, &argv));
    CHKPRINT(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    CHKPRINT(MPI_Comm_size(MPI_COMM_WORLD, &num_procs));
    
    // Set window allocation to use storage for odd processes
    if (IS_ODD_NUM(rank))
    {
        printf("Rank %d using storage for the window allocation.\n", rank);
        
        CHKPRINT(createDefaultInfo(rank, 0, &info));
    }
    
    // Allocate the window with at least num_procs bytes (i.e., depending on the page size, it can be more)
    CHKPRINT(MPI_Win_allocate(num_procs*sizeof(int), sizeof(int), info, MPI_COMM_WORLD, (void**)&baseptr, &win));
    
    // Put some test values on storage-allocated windows
    if (!IS_ODD_NUM(rank))
    {
        printf("Rank %d putting values to odd processes.\n", rank);
        
        for(int i = 1; i < num_procs; i += 2)
        {
            int value = rank + i;
            
            CHKPRINT(MPI_Win_lock(MPI_LOCK_SHARED, i, 0, win));
            CHKPRINT(MPI_Put(&value, 1, MPI_INT, i, rank, 1, MPI_INT, win));
            CHKPRINT(MPI_Win_unlock(i, win));
        }
    }
    
    // Force all processes to wait before printing the result
    CHKPRINT(MPI_Barrier(MPI_COMM_WORLD));
    
    if (IS_ODD_NUM(rank))
    {
        char  output[PATH_MAX];
        char *p_output = output;
        char *p_limit  = p_output + PATH_MAX;
        
        for(int i = 0; p_output != p_limit && i < num_procs; i += 2)
        {
            p_output += sprintf(p_output, "%d ", baseptr[i]);
        }
        
        printf("Rank %d values from even processes: %s\n", rank, output);
    }
    
    // The following two calls must fail (i.e., we are not owners of the allocation)
    //CHKPRINT(MPI_Win_detach(win, baseptr));
    //CHKPRINT(MPI_Free_mem(baseptr));
    
    // Release the window and finalize the MPI session
    CHKPRINT(MPI_Win_free(&win)); 
    CHKPRINT(MPI_Finalize());
    
    return MPI_SUCCESS;
}

