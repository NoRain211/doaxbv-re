#ifndef DOAXBV_XBOX_MEMORY_LAYOUT_H
#define DOAXBV_XBOX_MEMORY_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

#define XBOX_STARTUP_THREAD_OBJECT 0x00740500u
#define XBOX_STARTUP_THREAD_STACK_SLOT 0x03ffffc0u

uint32_t xbox_HeapAlloc(uint32_t size, uint32_t alignment);
void xbox_HeapFree(uint32_t guest_address);
uint32_t xbox_HeapCheckpoint(void);
bool xbox_HeapRestore(uint32_t checkpoint);

#endif
