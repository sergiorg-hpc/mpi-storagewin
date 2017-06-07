#ifndef _MFILE_H
#define _MFILE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Structure that defines a memory-file object, which is used to map files
 * in storage to memory.
 */
typedef struct
{
    char*  filename;    // Filename of the mapped-file (including path)
    size_t offset;      // Offset within the file
    size_t length;      // Length of the mapping
    int    unlink;      // Flag that determines if the file has to be deleted
    void*  addr;        // Address in memory of the mapping
    void*  addr_src;    // Address in memory of the mapping, as requested (unaligned)
} MFILE;

/**
 * Allocates a file in storage and creates a map in memory.
 */
int mfalloc(char const *filename, size_t offset, size_t length, int unlink, int access_style,
            int file_flags, int file_perm, MFILE *mfile);

/**
 * Flushes to disk any change made to the mapped file in memory.
 */
int mfsync(MFILE mfile);

/**
 * Flushes to disk any change made to the mapped file in memory within the specified range.
 */
int mfsync_at(MFILE mfile, size_t offset, size_t length, int async);

/**
 * Releases the mapped allocation and removes the associated file.
 */
int mffree(MFILE mfile);

#ifdef __cplusplus
}
#endif

#endif

