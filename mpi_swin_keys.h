#ifndef _MPI_STORAGE_WINDOWS_KEYS_H
#define _MPI_STORAGE_WINDOWS_KEYS_H

// MPI Storage Windows keys
#define MPI_SWIN_ALLOC_TYPE     "alloc_type"                // Defines if the window is allocated in storage ({ "memory", "storage" })
#define MPI_SWIN_FILENAME       "storage_alloc_filename"    // Determines the file-path for the mapping
#define MPI_SWIN_OFFSET         "storage_alloc_offset"      // Specifies the offset inside the given file or device
#define MPI_SWIN_FACTOR         "storage_alloc_factor"      // Defines the allocation factor (i.e., the part on storage)
#define MPI_SWIN_ORDER          "storage_alloc_order"       // Defines the order of the allocation (i.e., first memory or storage)
#define MPI_SWIN_UNLINK         "storage_alloc_unlink"      // Allows to delete the file during window deallocation ({ "true", "false" })

// MPI I/O supported keys
#define MPI_IO_ACCESS_STYLE     "access_style"              // Defines the access style of the window
#define MPI_IO_FILE_PERM        "file_perm"                 // Sets the permission of the file mapping (OR of mode_t values - see "man open")
#define MPI_IO_STRIPING_FACTOR  "striping_factor"           // Number of I/O devices that the file should be stripped across
#define MPI_IO_STRIPING_UNIT    "striping_unit"             // Stripe unit used for the file

#endif

