#ifndef _CSC_INTERNAL_DEC_H_
#define _CSC_INTERNAL_DEC_H_

#include "csc_typedef.h"
#include <stddef.h>
#include <stdint.h>

// Filter functions
void CSC_Filters_Inverse_E89(uint8_t *src, uint32_t size);
void CSC_Filters_Inverse_Dict(uint8_t *src, uint32_t size, uint8_t *swap_buf);
void CSC_Filters_Inverse_Delta(uint8_t *src, uint32_t size, uint32_t chnNum,
                               uint8_t *swap_buf);

void CSC_Filters_Init();

#endif
