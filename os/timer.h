#ifndef TIMER_H
#define TIMER_H

#include "types.h"

#define TICKS_PER_SEC (100)
// QEMU
#define CPU_FREQ (12500000)

uint64 get_cycle();
void timer_init();
void set_next_timer();

typedef struct {
	uint64 sec; // 自 Unix 纪元起的秒数
	uint64 usec; // 微秒数
} TimeVal;
/*=========================begin===========================*/
uint64 get_time_now();
/*=========================================================*/
#endif // TIMER_H