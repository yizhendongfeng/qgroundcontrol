/****************************************************************************
 *
 * Copyright (c) 2013-2020 ShenHang Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name ShenHang nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file px4_custom_mode.h
 * ShenHang custom flight modes
 *
 */

#ifndef ShenHang_CUSTOM_MODE_H_
#define ShenHang_CUSTOM_MODE_H_

#include <stdint.h>

enum ShenHang_CUSTOM_MAIN_MODE {
    ShenHang_CUSTOM_MAIN_MODE_MANUAL = 1,
    ShenHang_CUSTOM_MAIN_MODE_ALTCTL,
    ShenHang_CUSTOM_MAIN_MODE_POSCTL,
    ShenHang_CUSTOM_MAIN_MODE_AUTO,
    ShenHang_CUSTOM_MAIN_MODE_ACRO,
    ShenHang_CUSTOM_MAIN_MODE_OFFBOARD,
    ShenHang_CUSTOM_MAIN_MODE_STABILIZED,
    ShenHang_CUSTOM_MAIN_MODE_RATTITUDE,
    ShenHang_CUSTOM_MAIN_MODE_SIMPLE /* unused, but reserved for future use */
};

enum ShenHang_CUSTOM_SUB_MODE_AUTO {
    ShenHang_CUSTOM_SUB_MODE_AUTO_READY = 1,
    ShenHang_CUSTOM_SUB_MODE_AUTO_TAKEOFF,
    ShenHang_CUSTOM_SUB_MODE_AUTO_LOITER,
    ShenHang_CUSTOM_SUB_MODE_AUTO_MISSION,
    ShenHang_CUSTOM_SUB_MODE_AUTO_RTL,
    ShenHang_CUSTOM_SUB_MODE_AUTO_LAND,
    ShenHang_CUSTOM_SUB_MODE_AUTO_RTGS,
    ShenHang_CUSTOM_SUB_MODE_AUTO_FOLLOW_TARGET,
    ShenHang_CUSTOM_SUB_MODE_AUTO_PRECLAND
};

enum ShenHang_CUSTOM_SUB_MODE_POSCTL {
    ShenHang_CUSTOM_SUB_MODE_POSCTL_POSCTL = 0,
    ShenHang_CUSTOM_SUB_MODE_POSCTL_ORBIT
};

union px4_custom_mode {
	struct {
		uint16_t reserved;
		uint8_t main_mode;
		uint8_t sub_mode;
	};
	uint32_t data;
	float data_float;
	struct {
		uint16_t reserved_hl;
		uint16_t custom_mode_hl;
	};
};

#endif /* ShenHang_CUSTOM_MODE_H_ */
