
#include "common.h"
#include "mfile.h"

#define MMAP_PROT  (PROT_READ  | PROT_WRITE | PROT_EXEC)
#define MMAP_FLAGS (MAP_SHARED | MAP_NORESERVE) // Note: MAP_NORESERVE is
                                                // needed to avoid swapping

size_t g_pagesize = 0;

#define ALIGN_OFFSET(offset) (((offset) / g_pagesize) * g_pagesize)

int mfalloc(char const *filename, size_t offset, size_t length, double factor,
            int order, int unlink, int access_style, int file_flags,
            int file_perm, MFILE *mfile)
{
    int     fd             = 0;
    size_t  offset_aligned = 0;
    int     filename_size  = 0;
    void*   addr           = NULL;
    int     file_exists    = FALSE;
    struct stat st;
    
    // Check if the file exists before proceeding
    file_exists = (access(filename, F_OK) == 0);
    
    // Open / create the file with the provided permissions
    fd = open(filename, file_flags, file_perm);
    CHKB(fd == ERROR);
    
    CHK(fstat(fd, &st));
    
    // Retrieve the page size and cache it, if required
    if (g_pagesize == 0)
    {
        g_pagesize = sysconf(_SC_PAGESIZE);
    }
    
    offset_aligned = ALIGN_OFFSET(offset);
    
    // If file exists, check if it was requested to map the full-length of the file
    if (file_exists)
    {
        if (length == 0)
        {
            offset_aligned = 0;
            length         = st.st_size;
        }
        else
        {
            length += (offset - offset_aligned);
        }
    }
    
    // Define the mapping given the file descriptor and set the access pattern
    if (length > 0)
    {
        void   *addr_tmp = NULL;
        size_t length_s  = (double)length * factor;
        size_t length_m  = 0;
        int    prot      = (file_flags & O_RDONLY) ? PROT_READ  :
                           (file_flags & O_WRONLY) ? PROT_WRITE :
                                                     MMAP_PROT;
        
        // Update the storage length based on the aligned offset
        if (order == 0)
        {
            length_m = ALIGN_OFFSET(length - length_s);
            length_s = length - length_m;
        }
        else
        {
            length_s = ALIGN_OFFSET(length_s);
            length_m = length - length_s;
        }
    
        // Truncate the content by taking into account the offset
        if ((offset_aligned + length_s) > st.st_size)
        {
            // Important: posix_fallocate is more efficient, but older versions of Lustre will
            // produce unexpected results and cause errors.
            
            CHK(ftruncate(fd, offset_aligned + length_s));
        }
        
        // Create an anonymous mapping to reserve the virtual addresses
        addr = mmap(NULL, length, MMAP_PROT, MMAP_FLAGS | MAP_ANONYMOUS, -1, 0);
        CHKB(addr == MAP_FAILED || munmap(addr, length) != MPI_SUCCESS);
        
        // Divide the virtual address range between memory and storage
        if (order == 0)
        {
            if (length_m > 0)
            {
                addr_tmp = (char *)mmap(addr, length_m, prot,
                                        MMAP_FLAGS | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
                CHKB(addr_tmp == MAP_FAILED);
            }
            
            addr_tmp = mmap(((char *)addr) + length_m, length_s, prot, MMAP_FLAGS | MAP_FIXED, fd,
                            offset_aligned);
            CHKB(addr_tmp == MAP_FAILED);
        
            CHK(madvise(addr_tmp, length_s, access_style));
        }
        else
        {
            addr_tmp = mmap(addr, length_s, prot, MMAP_FLAGS | MAP_FIXED, fd,
                            offset_aligned);
            CHKB(addr_tmp == MAP_FAILED);
        
            CHK(madvise(addr_tmp, length_s, access_style));
            
            if (length_m > 0)
            {
                addr_tmp = (char *)mmap(((char *)addr) + length_s, length_m, prot,
                                        MMAP_FLAGS | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
                CHKB(addr_tmp == MAP_FAILED);
            }
        }
    }
 
    CHK(close(fd));
    
    // Fill the output MFILE object with the mapping details
    mfile->addr     = addr;
    mfile->addr_src = (void *)((char *)addr + (offset - offset_aligned));
    mfile->offset   = offset_aligned;
    mfile->length   = length;
    filename_size   = sizeof(char) * (strlen(filename) + 1);
    mfile->filename = (char *)malloc(filename_size);
    mfile->unlink   = unlink;
    
    memcpy(mfile->filename, filename, filename_size);
    
    return MPI_SUCCESS;
}

int mfsync(MFILE mfile)
{
    return msync(mfile.addr, mfile.length, MS_SYNC);
}

int mfsync_at(MFILE mfile, size_t offset, size_t length, int async)
{
    const size_t offset_aligned = ALIGN_OFFSET(offset);
    
    // Extend the requested length if the offset was aligned
    length += (offset - offset_aligned);
    
    return msync((char*)mfile.addr + offset_aligned, length, ((async) ? MS_ASYNC : MS_SYNC));
}

int mffree(MFILE mfile)
{
    // Remove any given permissions to the mapped-memory and unmap the file
    CHK(mprotect(mfile.addr, mfile.length, PROT_NONE));
    CHK(munmap(mfile.addr, mfile.length));
    
    // Check if the file was created for the mapping and delete it
    if (mfile.unlink)
    {
        CHK(unlink(mfile.filename));
    }
    
    free(mfile.filename);
    
    return MPI_SUCCESS;
}

