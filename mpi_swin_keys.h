#ifndef _MPI_STORAGE_WINDOWS_KEYS_H
#define _MPI_STORAGE_WINDOWS_KEYS_H

#define MPI_SWIN_ENABLED  "storage_alloc"           // Defines if the window is allocated in storage ({ "true", "false" })
#define MPI_SWIN_FILENAME "storage_alloc_filename"  // Determines the file-path for the mapping
#define MPI_SWIN_OFFSET   "storage_alloc_offset"    // Specifies the offset inside the given file
#define MPI_SWIN_UNLINK   "storage_alloc_unlink"    // Allows to delete the file during window deallocation ({ "true", "false" })

#endif

