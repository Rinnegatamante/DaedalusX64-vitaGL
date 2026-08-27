#ifndef TOUCH_ZONE_MAPPING_H
#define TOUCH_ZONE_MAPPING_H

#include <stdint.h>
#include <psp2/touch.h>
#include <psp2/ctrl.h>

#define MAX_TOUCH_ZONES 8

enum HitboxShape {
    HITBOX_RECTANGLE = 0,
    HITBOX_SQUARE = 1,
    HITBOX_CIRCLE = 2
};

struct TouchZone {
    bool enabled;
    char name[32];
    uint8_t shape;          // 0=Rectangle, 1=Square, 2=Circle
    uint16_t center_x;      // 0 .. 1920
    uint16_t center_y;      // 0 .. 1088
    uint16_t radius_w;      // Half-width / radius
    uint16_t radius_h;      // Half-height
    uint32_t mapped_button; // SCE_CTRL_...
};

extern TouchZone gTouchZones[MAX_TOUCH_ZONES];
extern bool gTouchZonesActive;
extern bool gCanvasEditMode;
extern int gSelectedTouchZone;
extern SceTouchData gFrontTouchData;

void InitDefaultTouchZones();
void LoadTouchZonesConfig();
void SaveTouchZonesConfig();
uint32_t EvaluateFrontTouchZones(const SceTouchData &touch);
void ProcessCanvasEditInput();

#endif // TOUCH_ZONE_MAPPING_H
