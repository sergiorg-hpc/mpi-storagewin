
#include "common.h"
#include "mfile.h"
#include "mpiwrappers_util.h"
#include "mpiwrappers.h"
//#ifdef MPI_SWIN_LUSTRE
//#include <lustre/lustreapi.h>
//#endif

///////////////////////////////////
// PRIVATE DEFINITIONS & METHODS //
///////////////////////////////////

/**
 * Helper method that allows to release a window allocation based on storage
 * or memory (default).
 */
int releaseWinAlloc(MPI_Win_Alloc *win_alloc)
{
    // Check the type of window allocation before releasing any data
    if (win_alloc->alloc_type == MPI_WIN_ALLOC_STORAGE)
    {
        MFILE *mfile = (MFILE *)win_alloc->data;
        
        DBGPRINTF("Storage mapping release with filename=\"%s\" offset=%zu length=%zu", mfile->filename, mfile->offset, mfile->length);
        
        CHK(mfsync(*mfile));
        CHK(mffree(*mfile));
        
        free(mfile);
    }
    else
    {
        DBGPRINT("Memory allocation release (replicating the default behaviour)");
        
        CHK(PMPI_Free_mem(win_alloc->data));
    }
    
    free(win_alloc);
    
    return MPI_SUCCESS;
}

/**
 * Callback function used when the given attributes of a certain window are being copied.
 */
int MPI_Win_copy_attr(MPI_Win oldwin, int win_keyval, void *extra_state, void *attribute_val_in, void *attribute_val_out, int *flag)
{
    return MPI_ERR_OP;
}

/**
 * Callback function used when the given attributes of a certain window are being released,
 * commonly happening when the window object itself is being released.
 */
int MPI_Win_release_attr(MPI_Win win, int win_keyval, void *attribute_val, void *extra_state)
{
    MPI_Win_Alloc *win_alloc = (MPI_Win_Alloc *)attribute_val;
    
    DBGPRINT("Release attribute callback function called");
    
    if (win_alloc->alloc_release)
    {
        DBGPRINT("Releasing the allocation attached to the window");
        
        CHK(releaseWinAlloc(win_alloc));
    }
    else
    {
        DBGPRINT("Adding the allocation to the cache (owned by the user)");
        
        CHK(addWinAlloc(win_alloc));
    }
    
    CHK(removeWinKeyval(win_keyval));
    
    return MPI_SUCCESS;
}

/**
 * Helper method that allows to attach a memory allocation to a window,
 * if needed (i.e., only if it was allocated with MPI_Alloc_mem).
 */
int cacheWinAlloc(MPI_Win win, void* base)
{
    MPI_Win_Alloc *win_alloc = NULL;
    int           win_keyval = 0;
    
    if (getWinAllocFromPtr(base, TRUE, &win_alloc) == MPI_SUCCESS)
    {
        DBGPRINT("Base pointer allocated internally with MPI_Alloc_mem (caching allocation in window)");
        
        // Associate the allocation data to the window object as an attribute, which
        // will allow to release the mapped-memory afterwards (if needed)
        CHK(MPI_Win_create_keyval(MPI_Win_copy_attr, MPI_Win_release_attr, &win_keyval, NULL));
        CHK(MPI_Win_set_attr(win, win_keyval, (void*)win_alloc));
        
        DBGPRINTF("Window attribute set with type=%s", ((win_alloc->alloc_type == MPI_WIN_ALLOC_MEM) ? "MEM" : "STORAGE"));
        
        CHK(addWinKeyval(win_keyval));
    }
    
    return MPI_SUCCESS;
}

/**
 * Helper method that allows to detach a memory allocation from a window,
 * if needed (i.e., only if it was allocated with MPI_Alloc_mem).
 */
int uncacheWinAlloc(MPI_Win win, const void* base)
{
    int win_keyval = 0;
    
    if (getWinKeyvalFromWinPtr(win, (void*)base, &win_keyval) == MPI_SUCCESS)
    {
        DBGPRINT("Base pointer found internally within the window (uncaching allocation in window)");
        
        // Disassociate the allocation data to the window object as an attribute (note
        // that the callback will be triggered, which will cache the alloc)
        CHK(MPI_Win_delete_attr(win, win_keyval));
        
        DBGPRINT("Window attribute unset from the window");
    }
    
    return MPI_SUCCESS;
}


//////////////////////////////////
// PUBLIC DEFINITIONS & METHODS //
//////////////////////////////////

int MPI_Alloc_mem(MPI_Aint size, MPI_Info info, void *baseptr)
{
    MPI_Win_Alloc   *win_alloc  = NULL;
    MPI_Info_Values info_values = { 0 };
    
    DBGPRINT("MPI allocation wrapper called");
    
    win_alloc = (MPI_Win_Alloc *)calloc(1, sizeof(MPI_Win_Alloc));
    
    // Parse the MPI_Info object to determine if the allocation has to be based
    // in traditional RAM memory or storage
    parseInfo(info, &info_values);
    
    if (info_values.alloc_type == MPI_WIN_ALLOC_STORAGE)
    {
        MFILE *mfile = NULL;
        
        DBGPRINTF("Storage allocation requested with filename=\"%s\" (offset=%zu unlink=%d)", info_values.filename,
                                                                                              info_values.offset,
                                                                                              info_values.unlink);
        
#ifdef MPI_SWIN_LUSTRE
        // Set the striping values for the file if it does not exist
        if (access(info_values.filename, F_OK) == ERROR && (info_values.striping_factor != 0 ||
                                                            info_values.striping_unit   != 0))
        {
            // Note: The Lustre support using the low-level API has been temporary removed after finding
            //       some compilation issues on a Cray XC40 machine. We will investigate the reasons and
            //       enable the support when a generic solution is available (e.g., using ioctl).
            //
            // int fd = llapi_file_open(info_values.filename, info_values.file_flags, info_values.file_perm,
            //                          info_values.striping_unit, -1, info_values.striping_factor, 0);
            // CHKB(fd == ERROR);
            // CHK(close(fd));
            
            char lfs_command[PATH_MAX];
            
            sprintf(lfs_command, "lfs setstripe -c %d -s %ld %s", info_values.striping_factor,
                                                                  info_values.striping_unit,
                                                                  info_values.filename);
            
            CHK(system(lfs_command));
        }
#endif
        
        // Create the mapping of the given file into memory
        mfile = (MFILE *)malloc(sizeof(MFILE));
        CHK(mfalloc(info_values.filename, info_values.offset, size, info_values.unlink, info_values.access_style,
                    info_values.file_flags, info_values.file_perm, mfile));
        
        // Fill the window allocation object with the mapping details (note that
        // the address returned matches the original request and is not aligned)
        win_alloc->alloc_type = MPI_WIN_ALLOC_STORAGE;
        win_alloc->data       = mfile;
        *((void**)baseptr)    = mfile->addr_src;
        
        DBGPRINTF("Allocation in storage successful with length=%lu", mfile->length);
    }
    else
    {
        DBGPRINT("Memory allocation requested (replicating the default behaviour)");
        
        // Allocate the requested size using MPI functionality
        CHK(PMPI_Alloc_mem(size, info, (void*)&win_alloc->data));
        
        // Fill the window allocation object with the allocation details
        win_alloc->alloc_type = MPI_WIN_ALLOC_MEM;
        *((void**)baseptr)    = win_alloc->data;
        
        DBGPRINTF("Allocation in memory successful with length=%ld", size);
    }
    
    // Disable the release flag to avoid issues if the user is allocating this memory
    // from outside (i.e., it will not be released during window deallocation)
    win_alloc->alloc_release = FALSE;
    
    return addWinAlloc(win_alloc);
}

int MPI_Free_mem(void *base)
{
    MPI_Win_Alloc *win_alloc = NULL;
    
    DBGPRINT("MPI allocation release wrapper called");
    
    if (getWinAllocFromPtr(base, TRUE, &win_alloc) == MPI_SUCCESS)
    {
        return releaseWinAlloc(win_alloc);
    }
    
    DBGPRINT("MPI allocation unknown");
    
    return MPI_ERR_BASE;
}

int MPI_Win_create(void *base, MPI_Aint size, int disp_unit, MPI_Info info, 
                   MPI_Comm comm, MPI_Win *win)
{
    DBGPRINT("Window creation wrapper called");
    
    CHK(PMPI_Win_create(base, size, disp_unit, info, comm, win));
    
    return cacheWinAlloc(*win, base);
}

int MPI_Win_allocate(MPI_Aint size, int disp_unit, MPI_Info info, 
                     MPI_Comm comm, void *baseptr, MPI_Win *win)
{
    MPI_Win_Alloc *win_alloc = NULL;
    
    DBGPRINT("Window allocation wrapper called");
    
    CHK(MPI_Alloc_mem(size, info, baseptr));
    CHK(MPI_Win_create(*((void**)baseptr), size, disp_unit, info, comm, win));
    
    // Enable the release flag to guarantee that the memory is released afterwards during
    // window deallocation (i.e., by default, the user has to manually release it)
    CHK(getWinAllocFromWin(*win, FALSE, &win_alloc));
    win_alloc->alloc_release = TRUE;
    
    DBGPRINTF("Window allocated succesfully with type=%s", ((win_alloc->alloc_type == MPI_WIN_ALLOC_MEM) ? "MEM" : "STORAGE"));
    
    return MPI_SUCCESS;
}

int MPI_Win_sync(MPI_Win win)
{
    MPI_Win_Alloc **win_allocs = NULL;
    int           count        = 0;
    
    DBGPRINT("Window flushing wrapper called");
    
    CHK(PMPI_Win_sync(win));
    
    if ((getAllWinAllocFromWin(win, &win_allocs, &count) == MPI_SUCCESS))
    {
        int count_storage = 0;
        int count_mem     = 0;
        
        DBGPRINTF("Window allocations cached in the window (count=%d)", count);
        
        for (int walloc = 0; !(count_storage && count_mem) && walloc < count; walloc++)
        {
            if (win_allocs[walloc]->alloc_type == MPI_WIN_ALLOC_STORAGE)
            {
                CHK(mfsync(*((MFILE *)win_allocs[walloc]->data)));
                
                count_storage++;
            }
            else
            {
                count_mem++;
            }
        }
        
        DBGPRINTF("Finished flushing / checking the allocation types (count_storage=%d / count_mem=%d)", count_storage, count_mem);
        
        free(win_allocs);
        
        // If we detect allocations in both memory and storage combined, the result
        // of the operation is unknown and we should return an error
        if (count_storage && count_mem)
        {
            return MPI_ERR_INTERN;
        }
    }
    
    return MPI_SUCCESS;
}

int MPI_Win_attach(MPI_Win win, void *base, MPI_Aint size)
{
    DBGPRINT("Window attach wrapper called");
    
    CHK(cacheWinAlloc(win, base));
    
    return PMPI_Win_attach(win, base, size);
}

int MPI_Win_detach(MPI_Win win, const void *base)
{
    DBGPRINT("Window detach wrapper called");
    
    CHK(uncacheWinAlloc(win, base));
    
    return PMPI_Win_detach(win, base);
}

