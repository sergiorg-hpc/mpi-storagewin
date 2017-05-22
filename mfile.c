
#include "common.h"
#include "mfile.h"

int g_pagesize = 0;

int mfalloc(char const *filename, size_t offset, size_t length, int unlink, MFILE *mfile)
{
    int     fd             = 0;
    size_t  offset_aligned = 0;
    int     filename_size  = 0;
    void*   addr           = NULL;
    int     file_exists    = FALSE;
    struct stat st;
    
    // Check if the file exists before proceeding
    file_exists = (access(filename, F_OK) == 0);
    
    // Open / create the file with RW permission, giving the calling user permission
    fd = open(filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    CHKB(fd == ERROR);
    
    CHK(fstat(fd, &st));
    
    // Retrieve the page size and cache it, if required
    if (g_pagesize == 0)
    {
        g_pagesize = sysconf(_SC_PAGESIZE);
    }
    
    offset_aligned = ((size_t)(offset / (size_t)g_pagesize)) * g_pagesize;
    
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
    
    // Truncate the content by taking into account the offset
    if ((offset_aligned + length) > st.st_size)
    {
        // Important: posix_fallocate is more efficient, but older versions of Lustre will
        // produce unexpected results and cause errors.
        
        CHK(ftruncate(fd, offset_aligned + length));
    }
    
    // Define the mapping given the file descriptor
    if (length > 0)
    {
        addr = mmap(NULL, length, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED | MAP_NORESERVE, fd, offset_aligned); // Note: MAP_NORESERVE is needed to avoid swapping
        CHKB(addr == MAP_FAILED);
    }
 
    CHK(close(fd));
    
    // Fill the output MFILE object with the mapping details
    mfile->addr     = addr;
    mfile->addr_src = (void *)(((char *)addr) + (offset - offset_aligned));
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
    size_t offset_aligned = ((size_t)(offset / (size_t)g_pagesize)) * g_pagesize;
    
    // Extend the requested length if the offset was aligned
    length += (offset - offset_aligned);
    
    return msync(&((char*)mfile.addr)[offset_aligned], length, ((async) ? MS_ASYNC : MS_SYNC));
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

