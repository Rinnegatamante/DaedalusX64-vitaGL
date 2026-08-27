#include "TouchZoneMapping.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

TouchZone gTouchZones[MAX_TOUCH_ZONES];
bool gTouchZonesActive = true;
bool gCanvasEditMode = false;
int gSelectedTouchZone = 0;
SceTouchData gFrontTouchData = {0};

extern bool touch_zones_window;
extern bool show_menubar;

static uint32_t last_buttons = 0;
static uint32_t last_touch_num = 0;

void InitDefaultTouchZones()
{
    for (int i = 0; i < MAX_TOUCH_ZONES; i++) {
        gTouchZones[i].enabled = false;
        snprintf(gTouchZones[i].name, sizeof(gTouchZones[i].name), "Zone %d", i + 1);
        gTouchZones[i].shape = HITBOX_RECTANGLE;
        gTouchZones[i].center_x = 960;
        gTouchZones[i].center_y = 544;
        gTouchZones[i].radius_w = 150;
        gTouchZones[i].radius_h = 150;
        gTouchZones[i].mapped_button = SCE_CTRL_L1;
    }

    // Default Preset Zone 1: Top-Left corner -> L1
    gTouchZones[0].enabled = false;
    snprintf(gTouchZones[0].name, sizeof(gTouchZones[0].name), "Corner L1");
    gTouchZones[0].shape = HITBOX_RECTANGLE;
    gTouchZones[0].center_x = 200;
    gTouchZones[0].center_y = 175;
    gTouchZones[0].radius_w = 200;
    gTouchZones[0].radius_h = 175;
    gTouchZones[0].mapped_button = SCE_CTRL_L1;

    // Default Preset Zone 2: Top-Right corner -> R1
    gTouchZones[1].enabled = false;
    snprintf(gTouchZones[1].name, sizeof(gTouchZones[1].name), "Corner R1");
    gTouchZones[1].shape = HITBOX_RECTANGLE;
    gTouchZones[1].center_x = 1720;
    gTouchZones[1].center_y = 175;
    gTouchZones[1].radius_w = 200;
    gTouchZones[1].radius_h = 175;
    gTouchZones[1].mapped_button = SCE_CTRL_R1;
}

void LoadTouchZonesConfig()
{
    InitDefaultTouchZones();

    FILE *f = fopen("ux0:data/DaedalusX64/touch_zones.ini", "r");
    if (!f) {
        f = fopen("data/DaedalusX64/touch_zones.ini", "r");
    }
    if (!f) return;

    char line[256];
    int current_zone = -1;

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\r' || line[0] == '\n') continue;

        int zone_idx = -1;
        if (sscanf(line, "[Zone_%d]", &zone_idx) == 1 || sscanf(line, "[Zone %d]", &zone_idx) == 1) {
            if (zone_idx >= 0 && zone_idx < MAX_TOUCH_ZONES) {
                current_zone = zone_idx;
            }
            continue;
        }

        if (current_zone >= 0 && current_zone < MAX_TOUCH_ZONES) {
            int enabled_val = 0;
            int shape_val = 0;
            unsigned int button_val = 0;
            unsigned int cx, cy, rw, rh;

            if (sscanf(line, "enabled=%d", &enabled_val) == 1) {
                gTouchZones[current_zone].enabled = (enabled_val != 0);
            } else if (sscanf(line, "shape=%d", &shape_val) == 1) {
                if (shape_val < 0 || shape_val > 2) shape_val = 0;
                gTouchZones[current_zone].shape = (uint8_t)shape_val;
            } else if (sscanf(line, "button=%u", &button_val) == 1) {
                gTouchZones[current_zone].mapped_button = button_val;
            } else if (sscanf(line, "center=%u,%u", &cx, &cy) == 2) {
                gTouchZones[current_zone].center_x = (uint16_t)cx;
                gTouchZones[current_zone].center_y = (uint16_t)cy;
            } else if (sscanf(line, "radii=%u,%u", &rw, &rh) == 2) {
                gTouchZones[current_zone].radius_w = (uint16_t)rw;
                gTouchZones[current_zone].radius_h = (uint16_t)rh;
            } else if (sscanf(line, "name=%31[^\r\n]", gTouchZones[current_zone].name) == 1) {
                gTouchZones[current_zone].name[31] = '\0';
            }
        }

        int active_val = 0;
        if (sscanf(line, "global_active=%d", &active_val) == 1) {
            gTouchZonesActive = (active_val != 0);
        }
    }

    fclose(f);

    // Sanitize bounds
    for (int i = 0; i < MAX_TOUCH_ZONES; i++) {
        if (gTouchZones[i].shape > 2) gTouchZones[i].shape = 0;
        if (gTouchZones[i].radius_w < 10) gTouchZones[i].radius_w = 50;
        if (gTouchZones[i].radius_h < 10) gTouchZones[i].radius_h = 50;
        gTouchZones[i].name[31] = '\0';
    }
}

void SaveTouchZonesConfig()
{
    FILE *f = fopen("ux0:data/DaedalusX64/touch_zones.ini", "w");
    if (!f) {
        f = fopen("data/DaedalusX64/touch_zones.ini", "w");
    }
    if (!f) return;

    fprintf(f, "# DaedalusX64 Touch Zone Mapping Config\n");
    fprintf(f, "global_active=%d\n\n", gTouchZonesActive ? 1 : 0);

    for (int i = 0; i < MAX_TOUCH_ZONES; i++) {
        if (gTouchZones[i].shape > 2) gTouchZones[i].shape = 0;
        gTouchZones[i].name[31] = '\0';
        fprintf(f, "[Zone_%d]\n", i);
        fprintf(f, "enabled=%d\n", gTouchZones[i].enabled ? 1 : 0);
        fprintf(f, "name=%s\n", gTouchZones[i].name[0] ? gTouchZones[i].name : "Zone");
        fprintf(f, "shape=%u\n", gTouchZones[i].shape);
        fprintf(f, "button=%u\n", gTouchZones[i].mapped_button);
        fprintf(f, "center=%u,%u\n", gTouchZones[i].center_x, gTouchZones[i].center_y);
        fprintf(f, "radii=%u,%u\n\n", gTouchZones[i].radius_w, gTouchZones[i].radius_h);
    }

    fclose(f);
}

uint32_t EvaluateFrontTouchZones(const SceTouchData &touch)
{
    if (!gTouchZonesActive || gCanvasEditMode) return 0;

    uint32_t buttons = 0;

    for (uint32_t i = 0; i < touch.reportNum; i++) {
        int tx = (int)touch.report[i].x;
        int ty = (int)touch.report[i].y;

        for (int z = 0; z < MAX_TOUCH_ZONES; z++) {
            if (!gTouchZones[z].enabled) continue;

            int dx = tx - (int)gTouchZones[z].center_x;
            int dy = ty - (int)gTouchZones[z].center_y;

            bool inside = false;

            if (gTouchZones[z].shape == HITBOX_CIRCLE) {
                int radius = (int)gTouchZones[z].radius_w;
                inside = (dx * dx + dy * dy) <= (radius * radius);
            } else if (gTouchZones[z].shape == HITBOX_SQUARE) {
                int half_size = (int)gTouchZones[z].radius_w;
                inside = (abs(dx) <= half_size) && (abs(dy) <= half_size);
            } else { // HITBOX_RECTANGLE
                int half_w = (int)gTouchZones[z].radius_w;
                int half_h = (int)gTouchZones[z].radius_h;
                inside = (abs(dx) <= half_w) && (abs(dy) <= half_h);
            }

            if (inside) {
                buttons |= gTouchZones[z].mapped_button;
            }
        }
    }

    return buttons;
}

void ProcessCanvasEditInput()
{
    if (!gCanvasEditMode) return;

    SceCtrlData pad;
    sceCtrlPeekBufferPositive(0, &pad, 1);

    SceTouchData touch;
    sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

    if (gSelectedTouchZone < 0 || gSelectedTouchZone >= MAX_TOUCH_ZONES) {
        gSelectedTouchZone = 0;
    }

    TouchZone &tz = gTouchZones[gSelectedTouchZone];
    if (tz.shape > 2) tz.shape = 0;

    // 1. Touch Tap Placement (Triggers ONLY on NEW touch tap down)
    if (touch.reportNum > 0 && last_touch_num == 0) {
        tz.center_x = touch.report[0].x;
        tz.center_y = touch.report[0].y;
        tz.enabled = true;
    }
    last_touch_num = touch.reportNum;

    uint32_t pressed = pad.buttons & ~last_buttons;
    last_buttons = pad.buttons;

    // 2. D-Pad (Flechas) Micro-Precision Movement System
    static uint32_t hold_l = 0, hold_r = 0, hold_u = 0, hold_d = 0;
    int dx = 0;
    int dy = 0;

    // D-Pad Left
    if (pad.buttons & SCE_CTRL_LEFT) {
        hold_l++;
        if (pressed & SCE_CTRL_LEFT) {
            dx -= 1; // Single press = exactly 1 pixel
        } else if (hold_l > 25 && (hold_l % 2 == 0)) {
            dx -= 1; // Slow hold (1px every 2 frames)
        }
    } else {
        hold_l = 0;
    }

    // D-Pad Right
    if (pad.buttons & SCE_CTRL_RIGHT) {
        hold_r++;
        if (pressed & SCE_CTRL_RIGHT) {
            dx += 1; // Single press = exactly 1 pixel
        } else if (hold_r > 25 && (hold_r % 2 == 0)) {
            dx += 1; // Slow hold (1px every 2 frames)
        }
    } else {
        hold_r = 0;
    }

    // D-Pad Up
    if (pad.buttons & SCE_CTRL_UP) {
        hold_u++;
        if (pressed & SCE_CTRL_UP) {
            dy -= 1; // Single press = exactly 1 pixel
        } else if (hold_u > 25 && (hold_u % 2 == 0)) {
            dy -= 1; // Slow hold (1px every 2 frames)
        }
    } else {
        hold_u = 0;
    }

    // D-Pad Down
    if (pad.buttons & SCE_CTRL_DOWN) {
        hold_d++;
        if (pressed & SCE_CTRL_DOWN) {
            dy += 1; // Single press = exactly 1 pixel
        } else if (hold_d > 25 && (hold_d % 2 == 0)) {
            dy += 1; // Slow hold (1px every 2 frames)
        }
    } else {
        hold_d = 0;
    }

    // Left Analog Stick Proportional Movement
    if (pad.lx < 98) {
        int delta = 98 - (int)pad.lx;
        dx -= (delta > 50) ? 3 : 1;
    } else if (pad.lx > 158) {
        int delta = (int)pad.lx - 158;
        dx += (delta > 50) ? 3 : 1;
    }

    if (pad.ly < 98) {
        int delta = 98 - (int)pad.ly;
        dy -= (delta > 50) ? 3 : 1;
    } else if (pad.ly > 158) {
        int delta = (int)pad.ly - 158;
        dy += (delta > 50) ? 3 : 1;
    }

    if (dx != 0 || dy != 0) {
        int new_cx = (int)tz.center_x + dx;
        int new_cy = (int)tz.center_y + dy;

        if (new_cx < 10) new_cx = 10;
        if (new_cx > 1910) new_cx = 1910;
        if (new_cy < 10) new_cy = 10;
        if (new_cy > 1078) new_cy = 1078;

        tz.center_x = (uint16_t)new_cx;
        tz.center_y = (uint16_t)new_cy;
        tz.enabled = true;
    }

    // 3. Zone Selection (L1 = Prev, R1 = Next)
    bool l1_pressed = (pressed & (SCE_CTRL_L1 | SCE_CTRL_LTRIGGER)) != 0;
    bool r1_pressed = (pressed & (SCE_CTRL_R1 | SCE_CTRL_RTRIGGER)) != 0;

    if (r1_pressed) {
        gSelectedTouchZone = (gSelectedTouchZone + 1) % MAX_TOUCH_ZONES;
    }
    if (l1_pressed) {
        gSelectedTouchZone = (gSelectedTouchZone + MAX_TOUCH_ZONES - 1) % MAX_TOUCH_ZONES;
    }

    // 4. Hitbox Sizing (Triangle ▲ = Enlarge, Cross ✖ = Shrink) with Micro-Precision System
    static uint32_t hold_tri = 0, hold_cross = 0;
    int dsize = 0;

    if (pad.buttons & SCE_CTRL_TRIANGLE) {
        hold_tri++;
        if (pressed & SCE_CTRL_TRIANGLE) {
            dsize += 1; // Single tap = exactly 1 pixel
        } else if (hold_tri > 25 && (hold_tri % 2 == 0)) {
            dsize += 1; // Smooth hold = 1 pixel every 2 frames
        }
    } else {
        hold_tri = 0;
    }

    if (pad.buttons & SCE_CTRL_CROSS) {
        hold_cross++;
        if (pressed & SCE_CTRL_CROSS) {
            dsize -= 1; // Single tap = exactly 1 pixel
        } else if (hold_cross > 25 && (hold_cross % 2 == 0)) {
            dsize -= 1; // Smooth hold = 1 pixel every 2 frames
        }
    } else {
        hold_cross = 0;
    }

    if (dsize != 0) {
        int new_w = (int)tz.radius_w + dsize;
        if (new_w < 10) new_w = 10;
        if (new_w > 800) new_w = 800;
        tz.radius_w = (uint16_t)new_w;

        if (tz.shape == HITBOX_CIRCLE || tz.shape == HITBOX_SQUARE) {
            tz.radius_h = tz.radius_w;
        } else {
            int new_h = (int)tz.radius_h + dsize;
            if (new_h < 10) new_h = 10;
            if (new_h > 500) new_h = 500;
            tz.radius_h = (uint16_t)new_h;
        }
    }

    // Right Analog Stick: Independent Width (RX) and Height (RY) adjustment for Rectangle shape
    if (tz.shape == HITBOX_RECTANGLE) {
        if (pad.rx < 98) {
            int delta = 98 - (int)pad.rx;
            int dw = (delta > 50) ? -2 : -1;
            if ((int)tz.radius_w + dw >= 10) tz.radius_w += dw;
        } else if (pad.rx > 158) {
            int delta = (int)pad.rx - 158;
            int dw = (delta > 50) ? 2 : 1;
            if (tz.radius_w + dw <= 800) tz.radius_w += dw;
        }

        if (pad.ry < 98) {
            int delta = 98 - (int)pad.ry;
            int dh = (delta > 50) ? -2 : -1;
            if ((int)tz.radius_h + dh >= 10) tz.radius_h += dh;
        } else if (pad.ry > 158) {
            int delta = (int)pad.ry - 158;
            int dh = (delta > 50) ? 2 : 1;
            if (tz.radius_h + dh <= 500) tz.radius_h += dh;
        }
    }

    // 5. Square (■): Cycle Shape (Rectángulo -> Cuadrado -> Círculo)
    if (pressed & SCE_CTRL_SQUARE) {
        tz.shape = (tz.shape + 1) % 3;
        if (tz.shape == HITBOX_CIRCLE || tz.shape == HITBOX_SQUARE) {
            tz.radius_h = tz.radius_w;
        }
    }

    // 6. Circle (🔴): Toggle Enable/Disable
    if (pressed & SCE_CTRL_CIRCLE) {
        tz.enabled = !tz.enabled;
    }

    // 7. START: Save & Exit Canvas Edit Mode -> Re-open Mapping Menu
    if (pressed & SCE_CTRL_START) {
        SaveTouchZonesConfig();
        gCanvasEditMode = false;
        touch_zones_window = true;
        show_menubar = true;
    }
}
