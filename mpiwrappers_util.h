#ifndef _MPIWRAPPERS_UTIL_H
#define _MPIWRAPPERS_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Macro that allows to declare functions for managing the internal cache of a
 * certain type (e.g., MPI_Win_Alloc cache).
 */
#define DECLARE_STORE_FN(S,T,SN) \
    int addWin##S(T win_##SN); \
    int removeWin##S(T win_##SN);

/**
 * Structure that contains the parsed values of a certain MPI_Info object, in
 * the context of MPI storage windows.
 */
typedef struct
{
    int     enabled;                    // Flag that determines if storage allocation is enabled
    int     unlink;                     // Flag that determines if the mapped file has to be deleted afterwards
    size_t  offset;                     // Offset within the file where the mapping begins
    char    filename[MPI_MAX_INFO_VAL]; // Requested filename for the mapped file (including path)
} MPI_Info_Values;

/**
 * Enumerate that defines the type of storage associated to the allocated window,
 * useful to differentiate between memory-based and storage-based windows.
 */
typedef enum
{
    ALLOCTYPE_MEM, ALLOCTYPE_STORAGE
} MPI_Win_Alloc_Type;

/**
 * Structure that represents the associated allocated data of a certain window.
 */
typedef struct
{
    MPI_Win_Alloc_Type alloc_type;      // Type of the allocation
    int                alloc_release;   // Flag that determines if the allocation must be released
    void               *data;           // Data allocated for the window
} MPI_Win_Alloc;

/**
 * Helper method that allows to parse a given MPI_Info object and return the
 * values associated with the MPI storage windows scenario.
 */
int parseInfo(MPI_Info info, MPI_Info_Values *values);

/**
 * Adds / Removes a key / value of a window to the internal cache, with the
 * purpose  of allowing the retrieval of the MPI_Win_Alloc afterwards.
 */
DECLARE_STORE_FN(Keyval, int, keyval);

/**
 * Adds / Removes an MPI_Win_Alloc to the internal cache, with the purpose of 
 * allowing the retrieval of the allocation if MPI_Alloc_mem was used.
 */
DECLARE_STORE_FN(Alloc, MPI_Win_Alloc*, alloc);

/**
 * Retrieves the MPI_Win_Alloc from a given window, by testing the cached
 * key / values available.
 */
int getWinAllocFromWin(MPI_Win win, int delete_entry, MPI_Win_Alloc **win_alloc);

/**
 * Retrieves the MPI_Win_Alloc from a given pointer, by testing the cached
 * allocations that are not assigned to a window.
 */
int getWinAllocFromPtr(void* ptr, int delete_entry, MPI_Win_Alloc **win_alloc);

/**
 * Retrieves the key / value from a given window that matches the provided
 * pointer, by testing the cached key / values available.
 */
int getWinKeyvalFromWinPtr(MPI_Win win, void* ptr, int *win_keyval);

/**
 * Retrieves all the MPI_Win_Alloc from a given window, by testing the cached
 * key / values available.
 */
int getAllWinAllocFromWin(MPI_Win win, MPI_Win_Alloc ***win_allocs, int *count);

#ifdef __cplusplus
}
#endif

#endif

