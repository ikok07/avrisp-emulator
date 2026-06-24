#ifndef INCLUDE_APP_STATE
#define INCLUDE_APP_STATE

#include "usbd_cdc.h"
#include "usbd_def.h"

typedef struct {
    USBD_HandleTypeDef husbd;
} APP_State;

extern APP_State gAppState;

#endif /* INCLUDE_APP_STATE */