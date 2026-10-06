/* Windows: process_vm_readv on the own process (ReadProcessMemory: no fault on unmapped pages). */
#pragma once
#include "../woc_posix.h"
#define process_vm_readv woc_process_vm_readv
