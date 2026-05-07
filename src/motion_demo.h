#ifndef MOTION_DEMO_H
#define MOTION_DEMO_H

#include <stdbool.h>

#include "lvgl.h"

#define MOTION_DEMO_KEY_ADD 0x10001U
#define MOTION_DEMO_KEY_DELETE 0x10002U
#define MOTION_DEMO_KEY_STATUS 0x10003U
#define MOTION_DEMO_KEY_NEXT_PENDING 0x10004U
#define MOTION_DEMO_KEY_APPROVE 0x10005U
#define MOTION_DEMO_KEY_REJECT 0x10006U
#define MOTION_DEMO_KEY_ESCAPE 0x10007U

void motion_demo_create(lv_obj_t * parent);
void motion_demo_handle_key(uint32_t key);
void motion_demo_tick(uint32_t now_ms);
bool motion_demo_smoke_check(void);

#endif
