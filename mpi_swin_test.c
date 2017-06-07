
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
    char file_perm[PATH_MAX];
    
    // Define the path according to the rank of the process
    sprintf(filename,  "./mpi_swin_%d.win", rank);
    sprintf(offset,    "%zu", offset_orig);
    sprintf(file_perm, "%d", (S_IRUSR | S_IWUSR));

    CHK(MPI_Info_create(info));
    CHK(MPI_Info_set(*info, MPI_SWIN_ALLOC_TYPE,    "storage"));
    // CHK(MPI_Info_set(*info, MPI_SWIN_DEVICEID,      "921"));
    CHK(MPI_Info_set(*info, MPI_SWIN_FILENAME,      filename));
    CHK(MPI_Info_set(*info, MPI_SWIN_OFFSET,        offset));
    CHK(MPI_Info_set(*info, MPI_SWIN_UNLINK,        "false"));
    CHK(MPI_Info_set(*info, MPI_IO_ACCESS_STYLE,    "write_mostly"));
    CHK(MPI_Info_set(*info, MPI_IO_FILE_PERM,       file_perm));
    CHK(MPI_Info_set(*info, MPI_IO_STRIPING_FACTOR, "2"));
    CHK(MPI_Info_set(*info, MPI_IO_STRIPING_UNIT,   "65536"));
    
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
    MPI_Win  win       = MPI_WIN_NULL;
    MPI_Info info      = MPI_INFO_NULL;
    int      rank      = 0;
    int      num_procs = 0;
    int      *baseptr  = NULL;
    
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
    
    // Allocate the window with "num_procs" integers
    CHKPRINT(MPI_Win_allocate(num_procs * sizeof(int), sizeof(int), info,
                              MPI_COMM_WORLD, (void**)&baseptr, &win));
    
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
    
    // Synchronize the storage (just to test that this functionality still works)
    CHKPRINT(MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win));
    CHKPRINT(MPI_Win_sync(win));
    CHKPRINT(MPI_Win_unlock(rank, win));
    
    // Print the result in order
    for (int i = 0; i < num_procs; i++)
    {
        CHKPRINT(MPI_Barrier(MPI_COMM_WORLD));
        
        if (IS_ODD_NUM(rank) && rank == i)
        {
            printf("Rank %d values from even processes:", rank);
            
            for(int j = 0; j < num_procs; j += 2)
            {
                printf(" %d", baseptr[j]);
            }
            
            printf("\n");
        }
    }
    
    // Release the window and finalize the MPI session
    CHKPRINT(MPI_Win_free(&win)); 
    CHKPRINT(MPI_Finalize());
    
    return MPI_SUCCESS;
}

