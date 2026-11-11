#include "FastDAC.h"

uint32_t fastDacCleanReg1 = 0;
uint32_t fastDacCleanReg2 = 0;
portMUX_TYPE fastDacSpinlock = portMUX_INITIALIZER_UNLOCKED;