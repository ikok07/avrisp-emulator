#include "app_state.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_pcd.h"
void SysTick_Handler() { HAL_IncTick(); }
void OTG_FS_IRQHandler() { HAL_PCD_IRQHandler(gAppState.husbd.pData); }