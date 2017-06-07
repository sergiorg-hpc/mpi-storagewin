
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
 * ranks to have the allocation in storage. The example will create a dynamic
 * window and make each even process to write on the odd ones certain values.
 */
int main (int argc, char *argv[])
{
    MPI_Win  win         = MPI_WIN_NULL;
    MPI_Info info[2]     = { MPI_INFO_NULL, MPI_INFO_NULL };
    MPI_Aint disp[2]     = { 0 };
    MPI_Aint *disps[2]   = { NULL };
    MPI_Aint size        = 0;
    int      rank        = 0;
    int      num_procs   = 0;
    int      *baseptr[2] = { NULL };
    
    // Initialize MPI and retrieve the rank of the process
    CHKPRINT(MPI_Init(&argc, &argv));
    CHKPRINT(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    CHKPRINT(MPI_Comm_size(MPI_COMM_WORLD, &num_procs));
    
    size = num_procs * sizeof(int);
    
    // Set window allocation to use storage for odd processes
    if (IS_ODD_NUM(rank))
    {
        printf("Rank %d using storage for the window allocation.\n", rank);
        
        CHKPRINT(createDefaultInfo(rank, 0,    &info[0]));
        CHKPRINT(createDefaultInfo(rank, size, &info[1]));
    }
    
    // Create the dynamic window
    CHKPRINT(MPI_Win_create_dynamic(MPI_INFO_NULL, MPI_COMM_WORLD, &win));
    
    // Allocate space for "num_procs" integers and attach each allocation
    CHKPRINT(MPI_Alloc_mem(size, info[0], (void**)&baseptr[0]));
    CHKPRINT(MPI_Alloc_mem(size, info[1], (void**)&baseptr[1]));
    CHKPRINT(MPI_Win_attach(win, (void*)baseptr[0], size));
    CHKPRINT(MPI_Win_attach(win, (void*)baseptr[1], size));

    // Get the displacements for each allocation
    disps[0] = (MPI_Aint *)malloc(sizeof(MPI_Aint) * num_procs);
    MPI_Get_address(baseptr[0], &disp[0]);
    MPI_Allgather(&disp[0], 1, MPI_AINT, disps[0], 1, MPI_AINT, MPI_COMM_WORLD);
    
    disps[1] = (MPI_Aint *)malloc(sizeof(MPI_Aint) * num_procs);
    MPI_Get_address(baseptr[1], &disp[1]);
    MPI_Allgather(&disp[1], 1, MPI_AINT, disps[1], 1, MPI_AINT, MPI_COMM_WORLD);

    printf("Rank %d ready and displacements sent (disp[0]=%ld / disp[1]=%ld).\n", rank, disp[0], disp[1]);
        
    // Put some test values on storage-allocated windows
    if (!IS_ODD_NUM(rank))
    {
        for(int i = 1; i < num_procs; i += 2)
        {
            int value  = rank + i;
            int value2 = value * 2;
            
            printf("Rank %d putting values %d and %d to rank %d (disp[0]=%ld / disp[1]=%ld).\n", rank, value, value2, i,
                                                                                                 disps[0][i], disps[1][i]);

            CHKPRINT(MPI_Win_lock(MPI_LOCK_SHARED, i, 0, win));
            CHKPRINT(MPI_Put(&value, 1, MPI_INT, i, disps[0][i]  + rank*sizeof(int), 1, MPI_INT, win));
            CHKPRINT(MPI_Put(&value2, 1, MPI_INT, i, disps[1][i] + rank*sizeof(int), 1, MPI_INT, win));
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
                printf(" %d", baseptr[0][j]);
            }
            
            for(int j = 0; j < num_procs; j += 2)
            {
                printf(" %d", baseptr[1][j]);
            }
            
            printf("\n");
        }
    }
    
    // Release the allocated memory by detaching it first
    CHKPRINT(MPI_Win_detach(win, baseptr[0]));
    CHKPRINT(MPI_Free_mem(baseptr[0]));
    CHKPRINT(MPI_Win_detach(win, baseptr[1]));
    CHKPRINT(MPI_Free_mem(baseptr[1]));
    free(disps[0]); free(disps[1]);
    
    // Release the window and finalize the MPI session
    CHKPRINT(MPI_Win_free(&win));
    CHKPRINT(MPI_Finalize());
    
    return MPI_SUCCESS;
}

