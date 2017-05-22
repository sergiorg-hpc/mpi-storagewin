
#include "common.h"
#include "mfile.h"
#include "mpi_swin_keys.h"
#include "mpiwrappers_util.h"

///////////////////////////////////
// PRIVATE DEFINITIONS & METHODS //
///////////////////////////////////

#define NUM_WINDOWS_INIT 64

/**
 * Macro that defines the structure and functions for the internal cache of a
 * certain type (e.g., MPI_Win_Alloc cache).
 */
#define MAKE_STORE(S,T,SN) \
    struct Win##S##Store \
    { \
        T   *data; \
        int count; \
        int size; \
    } w##SN##_cache; \
    \
    int addWin##S(T win_##SN) \
    { \
        if (w##SN##_cache.data == NULL) \
        { \
            w##SN##_cache.data  = (T *)malloc(sizeof(T) * NUM_WINDOWS_INIT); \
            w##SN##_cache.count = 0; \
            w##SN##_cache.size  = NUM_WINDOWS_INIT; \
        } \
        else if (w##SN##_cache.count == w##SN##_cache.size) \
        { \
            w##SN##_cache.size <<= 1; \
            w##SN##_cache.data   = (T *)realloc(w##SN##_cache.data, sizeof(T) * w##SN##_cache.size); \
        } \
        \
        w##SN##_cache.data[w##SN##_cache.count++] = win_##SN; \
        \
        return MPI_SUCCESS; \
    } \
    \
    void deleteWin##S##Entry(int index) \
    { \
        memmove(&w##SN##_cache.data[index], &w##SN##_cache.data[index+1], sizeof(T) * (w##SN##_cache.count - index - 1)); \
        w##SN##_cache.count--; \
    }\
    \
    int removeWin##S(T win_##SN) \
    { \
        for (int i = 0; i < w##SN##_cache.count; i++) \
        { \
            if (w##SN##_cache.data[i] == win_##SN) \
            { \
                deleteWin##S##Entry(i); \
                \
                return MPI_SUCCESS; \
            } \
        } \
        \
        return MPI_ERR_OTHER; \
    }

/**
 * Helper method that allows to obtain the value of a given MPI_Info key.
 */
int getInfoValue(MPI_Info info, char* key, char* info_value)
{
    int  hr   = MPI_SUCCESS;
    int  flag = 0;
    
    hr = MPI_Info_get(info, key, (MPI_MAX_INFO_VAL - 1), info_value, &flag); // Note: Leaving space for null char (at the end)
    
    return (hr == MPI_SUCCESS && flag);
}

void* getPtrFromWinAlloc(MPI_Win_Alloc *win_alloc)
{
    return (win_alloc->alloc_type == ALLOCTYPE_MEM) ? win_alloc->data :
                                                      ((MFILE *)win_alloc->data)->addr_src;
}


//////////////////////////////////
// PUBLIC DEFINITIONS & METHODS //
//////////////////////////////////

int parseInfo(MPI_Info info, MPI_Info_Values *values)
{
    char info_value[MPI_MAX_INFO_VAL];
    
    // Set the default values for the hints
    values->enabled = FALSE;
    values->unlink  = FALSE;
    values->offset  = 0;
    
    // If we find the "enabled" flag and it's positive, retrieve the settings
    if (info != MPI_INFO_NULL && getInfoValue(info, MPI_SWIN_ENABLED, info_value) &&
        !strcmp(info_value, "true"))
    {
        CHKB(!getInfoValue(info, MPI_SWIN_FILENAME, info_value));
        strcpy(values->filename, info_value);
        
        if (getInfoValue(info, MPI_SWIN_OFFSET, info_value))
        {
            sscanf(info_value, "%zu", &values->offset);
        }
        
        if (getInfoValue(info, MPI_SWIN_UNLINK, info_value))
        {
            values->unlink = !strcmp(info_value, "true");
        }
        
        // If we reach this point, we have at least the filename for the mapping
        // and we can enable storage allocations
        values->enabled = TRUE;
    }
    
    return MPI_SUCCESS;
}

MAKE_STORE(Keyval, int, keyval);
MAKE_STORE(Alloc, MPI_Win_Alloc*, alloc);

int getWinAllocFromWin(MPI_Win win, int delete_entry, MPI_Win_Alloc **win_alloc)
{
    MPI_Win_Alloc *win_alloc_tmp = 0;
    int           flag           = 0;
    
    for (int keyval = 0; keyval < wkeyval_cache.count; keyval++)
    {
        CHK(MPI_Win_get_attr(win, wkeyval_cache.data[keyval], (void **)&win_alloc_tmp, &flag));
        
        if (flag)
        {
            *win_alloc = win_alloc_tmp;
            
            if (delete_entry)
            {
                deleteWinKeyvalEntry(keyval);
            }
            
            return MPI_SUCCESS;
        }
    }
    
    return MPI_ERR_KEYVAL;
}

int getWinAllocFromPtr(void* ptr, int delete_entry, MPI_Win_Alloc **win_alloc)
{
    for (int walloc = 0; walloc < walloc_cache.count; walloc++)
    {
        if (getPtrFromWinAlloc(walloc_cache.data[walloc]) == ptr)
        {
            *win_alloc = walloc_cache.data[walloc];
            
            if (delete_entry)
            {
                deleteWinAllocEntry(walloc);
            }
            
            return MPI_SUCCESS;
        }
    }
    
    return MPI_ERR_OTHER;
}

int getWinKeyvalFromWinPtr(MPI_Win win, void* ptr, int *win_keyval)
{
    MPI_Win_Alloc *win_alloc = NULL;
    int           flag       = 0;
    
    for (int keyval = 0; keyval < wkeyval_cache.count; keyval++)
    {
        CHK(MPI_Win_get_attr(win, wkeyval_cache.data[keyval], (void **)&win_alloc, &flag));
        
        if (flag && (getPtrFromWinAlloc(win_alloc) == ptr))
        {
            *win_keyval = wkeyval_cache.data[keyval];
            
            return MPI_SUCCESS;
        }
    }
    
    return MPI_ERR_KEYVAL;
}

int getAllWinAllocFromWin(MPI_Win win, MPI_Win_Alloc ***win_allocs, int *count)
{
    MPI_Win_Alloc **wallocs   = (MPI_Win_Alloc **)malloc(sizeof(MPI_Win_Alloc *) * wkeyval_cache.count);
    int           num_keyvals = 0;
    int           flag        = 0;
    
    for (int keyval = 0; keyval < wkeyval_cache.count; keyval++)
    {
        CHK(MPI_Win_get_attr(win, wkeyval_cache.data[keyval], (void **)&wallocs[num_keyvals], &flag));
        
        if (flag)
        {
            num_keyvals++;
        }
    }
    
    if (num_keyvals > 0)
    {
        *win_allocs = (MPI_Win_Alloc **)malloc(sizeof(MPI_Win_Alloc *) * num_keyvals);
        
        memcpy(*win_allocs, wallocs, sizeof(MPI_Win_Alloc *) * num_keyvals);
    }
    
    // Set the count to the number of key / values found (it can be 0)
    *count = num_keyvals;
    
    free(wallocs);
    
    return (num_keyvals > 0) ? MPI_SUCCESS : MPI_ERR_KEYVAL;
}

