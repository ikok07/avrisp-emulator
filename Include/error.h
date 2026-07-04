#ifndef INCLUDE_ERROR
#define INCLUDE_ERROR

#include <stdint.h>
void ERROR_Init();
void ERROR_Trigger(uint32_t DurationMs);
void ERROR_Fatal();

#endif /* INCLUDE_ERROR */
