# MPI Storage Windows

## Introduction
Upcoming HPC clusters will feature hybrid memories and storage devices per compute node. In the same way that storage will seamlessly extend memory in the near future, programming interfaces for memory operations will also become interfaces for I/O operations. In this regard, we propose to use the MPI one-sided communication model and MPI windows as unique interface for programming memory and storage.

This library is a small wrapper that extends any existing MPI implementation to support MPI *storage* windows. We take advantage of the MPI Profiling Interface (PMPI) to intercept certain operations of the MPI one-sided communication model, allowing to maintain the same interface defined in the standard. Hence, existing applications directly benefit from this extension with minor changes (see below).

## How to use the library
In order to use MPI storage windows, it is required that you compile your application with the library. For instance, some MPI implementations, such as MPICH, support the `-profile` option to establish the profiling library to be used for the application. We provide two test applications ([mpi_swin_test.c](mpi_swin_test.c) and [mpi_swin_test_dynamic.c](mpi_swin_test_dynamic.c)) that are compiled with the [Makefile](Makefile) of the library for further reference. Do not forget to define the "`USE_PROFILE`" variable if you are compiling the library with MPICH (i.e., `make USE_PROFILE=1`).

In addition, certain performance hints have to be provided to `MPI_Win_allocate` (in the case of MPI dynamic windows, the hints are expected on `MPI_Alloc_mem`). There is no risk of introducing these hints into an existing application, even while not using this library. The reason is that, if the specific MPI implementation does not support storage allocations, the performance hints are simply ignored. These are the hints required:

- `alloc_type`. This hint can be set to "`storage`" to enable the MPI window allocation on storage. Otherwise, the window will be allocated in memory (default).
- `storage_alloc_filename`. Defines the path and the name of the target file or block device. Relative paths are supported as well.
- `storage_alloc_offset`. Identifies the MPI storage window starting point inside a file, but is also valid when targeting block devices directly.
- `storage_alloc_factor`. Enables *combined* window allocations, where a single virtual address space contains both memory and storage. A value of "`0.5`" would associate the first half of the addresses into memory, and the second half into storage.
- `storage_alloc_unlink`. If set to "`true`", it removes the associated file during the deallocation of an MPI storage window (i.e., useful for writing temporary files).
- `storage_alloc_discard`. If set to "`true`", avoids to synchronize to storage the recent changes during the deallocation of the MPI storage window.

Note that providing the same path for different MPI storage windows allows MPI processes to write to / read from a shared file or block device. Thus, it is mandatory in this case that each process defines the offset to differentiate the starting point of the window. If overlapping regions exist, consistency cannot be guaranteed in all situations. By default, the offset is set to zero and the unlink flag to `false`, if not specified.

###### Performance Hints from MPI I/O
The library also supports some of the reserved hints of MPI I/O, such as `access_style`, `file_perm`, `striping_factor`, and `striping_unit`. These features are still experimental, so it is reasonable to expect some issues while combining some of these hints.

## Source Code Example
We refer to the [Makefile](Makefile) for an example on how to link your application with the library. We also provide two test applications ([mpi_swin_test.c](mpi_swin_test.c) and [mpi_swin_test_dynamic.c](mpi_swin_test_dynamic.c)) that demonstrate the use of MPI storage windows with both conventional and dynamic windows, respectively.

Nonetheless, below is illustrated a snippet that allocates a window by providing some of the mentioned performance hints:

```
// Define the MPI_Info object to enable storage allocations 
MPI_Info_create(&info); 
MPI_Info_set(info, "alloc_type", "storage");
MPI_Info_set(info, "storage_alloc_filename", "/path/to/file"); 
MPI_Info_set(info, "storage_alloc_offset", "0");
MPI_Info_set(info, "storage_alloc_unlink", "false"); 
   
// Allocate storage window with space for num_procs integers
MPI_Win_allocate(num_elems * sizeof(int), sizeof(int), info,
                 MPI_COMM_WORLD, (void**)&baseptr, &win);
...
```

Note how the `MPI_Win_allocate` remains unaltered and still follows the original specification defined in the MPI standard.

## Upcoming releases
The current library is still experimental. We expect to provide a new implementation that contains a custom memory-mapped IO implementation for heterogeneous allocations.
  
## Contributors
This project has been developed by **Sergio Rivas-Gomez** (sergiorg@kth.se), in collaboration with **Ivy Bo Peng** (bopeng@kth.se), **Stefano Markidis** (markidis@kth.se), **Erwin Laure** (erwinl@kth.se), **Gokcen Kestor** (gokcen.kestor@pnnl.gov), **Roberto Gioiosa** (roberto.gioiosa@pnnl.gov) and **Sai Narasimhamurthy** (sai.narasimhamurthy@seagate.com).

## Disclaimer
The library is considered an Alpha release, meaning that we cannot guarantee a bug-free implementation and a full compliance with the MPI standard in all cases. Use at your own responsibility, but we still thank you for doing so and kindly ask you to contact us if you encounter any issues.
