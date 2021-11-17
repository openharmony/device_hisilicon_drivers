/*
 * Copyright (c) 2020 HiSilicon (Shanghai) Technologies CO., LIMITED.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PWM_HI35XX_H
#define PWM_HI35XX_H
#include "hdf_base.h"

#define PWM_CLK_HZ     3000000 // 3MHz
#define PWM_CLK_PERIOD 333     // 333ns

#define PWM_MAX_HZ     1500000 // 1.5MHz
#define PWM_MIN_PERIOD 666     // 666ns

#define PWM_MIN_HZ     0.045       // 0.045Hz
#define PWM_MAX_PERIOD 22222222222 // 22222222222ns > 4294967295ns (UINT32_MAX)

#define PWM_ENABLE  1
#define PWM_DISABLE 0

#define PWM_INV_OFFSET  1
#define PWM_KEEP_OFFSET 2

#define PWM_DEFAULT_PERIOD     0x3E7 // 999
#define PWM_DEFAULT_POLARITY   0
#define PWM_DEFAULT_DUTY_CYCLE 0x14D // 333

struct HiPwmRegs {
    volatile uint32_t cfg0;
    volatile uint32_t cfg1;
    volatile uint32_t cfg2;
    volatile uint32_t ctrl;
    volatile uint32_t state0;
    volatile uint32_t state1;
    volatile uint32_t state2;
};

static inline void HiPwmDisable(struct HiPwmRegs *reg)
{
    reg->ctrl &= ~1;
}

static inline void HiPwmAlwaysOutput(struct HiPwmRegs *reg)
{
    /* keep the pwm always output */
    reg->ctrl |= ((1 << PWM_KEEP_OFFSET) | PWM_ENABLE);
}

static inline void HiPwmOutputNumberSquareWaves(struct HiPwmRegs *reg, uint32_t number)
{
    uint32_t mask;

    /* pwm output number square waves */
    reg->cfg2 = number;
    mask = ~(1 << PWM_KEEP_OFFSET);
    reg->ctrl &= mask;
    reg->ctrl |= PWM_ENABLE;
}

static inline void HiPwmSetPolarity(struct HiPwmRegs *reg, uint8_t polarity)
{
    uint32_t mask;

    mask = ~(1 << PWM_INV_OFFSET);
    reg->ctrl &= mask;
    reg->ctrl |= (polarity << PWM_INV_OFFSET);
}

static inline void HiPwmSetPeriod(struct HiPwmRegs *reg, uint32_t period)
{
    reg->cfg0 = period / PWM_CLK_PERIOD;
}

static inline void HiPwmSetDuty(struct HiPwmRegs *reg, uint32_t duty)
{
    reg->cfg1 = duty / PWM_CLK_PERIOD;
}

#endif /* PWM_HI35XX_H */
