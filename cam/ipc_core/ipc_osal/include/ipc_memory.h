#ifndef __IPC_MEMORY_H__
#define __IPC_MEMORY_H__
#ifdef __cplusplus
extern "C" {
#endif
#include <ipc_std.h>
/**
 * @brief Apply for virtual memory, the use method is enhanced compared with malloc
 *
 * @param total_size The size of the applied memory
 * @param clean_size The size of the memory that needs to be cleared to 0
 * @return Memory pointer, NULL means insufficient memory allocation failure
 * @note For example, ipc_malloc(20, 10); means applying for 20 bytes of memory and memsetting the first 10 bytes
 * @note This cluster of functions must not be mixed with malloc/free, because the internal implementation may be
 * changed
 */
EXAPI vptr ipc_malloc(u32 total_size, u32 clean_size);
/**
 * @brief Release virtual memory, using the same method as free
 *
 * @param h_mem Memory through ipc_malloc application
 */
EXAPI void ipc_free(vptr h_mem);
/**
 * @brief Resize the memory area block, using the same method as realloc
 *
 * @param h_mem Memory applied through ipc_malloc
 * @param new_size New memory size
 * @return New memory pointer, NULL means failed memory allocation due to insufficient memory, at that time, the old
 * h_mem is still valid and will not be released
 */
EXAPI vptr ipc_realloc(vptr h_mem, u32 new_size);
/**
 * @brief Apply for shared memory
 *
 * @param mark The commonly specified mark value (0-255), which serves as the absolute handle for manipulating this
 * shared memory
 * @param size The size of the applied memory
 * @param first_create Whether it is the first creation to address concurrent creation issues
 * @return Memory pointer, NULL means failed memory allocation due to insufficient memory
 */
EXAPI vptr ipc_shmalloc(u8 mark, u32 size, pu8 first_create);
/**
 * @brief Reference shared memory
 *
 * @param mark The commonly specified mark value (0-255), which serves as the absolute handle for manipulating this
 * shared memory
 * @return Memory pointer, NULL means that no one has called ipc_shmalloc for this mark and cannot be referenced
 * @note ipc_shmalloc can also use the same mark to obtain the same shared memory at the same time, the advantage of this
 * interface compared to ipc_shmalloc is that it focuses only on the reference of shared memory, and the memory size is
 * determined by the caller of ipc_shmalloc to avoid various problems caused by inconsistent settings of memory size on
 * both sides
 */
EXAPI vptr ipc_shmref(u8 mark);
/**
 * @brief Dereference shared memory
 *
 * @param h_mem Memory address referenced through ipc_shmref
 */
EXAPI void ipc_shmunref(vptr h_mem);
/**
 * @brief Completely destroy the memory applied for by ipc_shmalloc
 *
 * @param mark The commonly specified mark value (0-255), which serves as the absolute handle for manipulating this
 * shared memory
 * @param h_mem Memory applied through ipc_shmalloc
 * @note The shared memory will not be destroyed immediately until all references are cleared before it will be
 * completely released
 */
EXAPI void ipc_shmfree(u8 mark, vptr h_mem);
#ifdef __cplusplus
}
#endif
#endif //__IPC_MEMORY_H__
