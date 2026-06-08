#include "cinematica.h"
#include "config.h"

void differential_drive(float v, float w, float *v_left, float *v_right) {
    *v_left  = v - (w * WHEEL_BASE / 2.0);
    *v_right = v + (w * WHEEL_BASE / 2.0);
}