/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include <Arduino.h>
#include <M5PM1.h>

/*
 * NeoPixel 彩虹渐变示例（使用 GPIO0 作为 LED_EN/数据相关功能）。
 * NeoPixel rainbow demo (uses GPIO0 for LED_EN/data-related function).
 *
 * 注意：NeoPixel 仅支持 GPIO0，且 LED 数量最大 31（寄存器 5-bit 限制）。
 * Note: NeoPixel is only supported on GPIO0, and LED count max is 31 (5-bit limit).
 */

M5PM1 pm1;

#define LOGI(fmt, ...) Serial.printf("[PM1][I] " fmt "\r\n", ##__VA_ARGS__)
#define LOGW(fmt, ...) Serial.printf("[PM1][W] " fmt "\r\n", ##__VA_ARGS__)
#define LOGE(fmt, ...) Serial.printf("[PM1][E] " fmt "\r\n", ##__VA_ARGS__)

#ifndef PM1_I2C_SDA
#define PM1_I2C_SDA 47
#endif
#ifndef PM1_I2C_SCL
#define PM1_I2C_SCL 48
#endif
#ifndef PM1_I2C_FREQ
#define PM1_I2C_FREQ M5PM1_I2C_FREQ_100K
#endif

static const uint8_t LED_COUNT = 8;
static const uint8_t BRIGHTNESS = 64;

static void printDivider() {
    Serial.println("--------------------------------------------------");
}

static uint8_t scale(uint8_t v, uint8_t scale) {
    // 通过亮度系数缩放颜色，避免过亮。
    // Scale color by brightness to avoid excessive intensity.
    return static_cast<uint8_t>((static_cast<uint16_t>(v) * scale) / 255);
}

static m5pm1_rgb_t wheel(uint8_t pos) {
    pos = 255 - pos;
    m5pm1_rgb_t c = {0, 0, 0};
    if (pos < 85) {
        c.r = 255 - pos * 3;
        c.g = 0;
        c.b = pos * 3;
    } else if (pos < 170) {
        pos -= 85;
        c.r = 0;
        c.g = pos * 3;
        c.b = 255 - pos * 3;
    } else {
        pos -= 170;
        c.r = pos * 3;
        c.g = 255 - pos * 3;
        c.b = 0;
    }
    c.r = scale(c.r, BRIGHTNESS);
    c.g = scale(c.g, BRIGHTNESS);
    c.b = scale(c.b, BRIGHTNESS);
    return c;
}

void setup() {
    Serial.begin(115200);
    delay(200);
    printDivider();
    LOGI("NeoPixel demo start");

    m5pm1_err_t err = pm1.begin(&Wire, M5PM1_DEFAULT_ADDR, PM1_I2C_SDA, PM1_I2C_SCL, PM1_I2C_FREQ);
    if (err != M5PM1_OK) {
        LOGE("PM1 begin failed: %d", err);
        while (true) {
            delay(1000);
        }
    }

    // GPIO0 设置为 OTHER 以启用 NeoPixel；避免与 GPIO/IRQ/WAKE 冲突。
    // Set GPIO0 to OTHER for NeoPixel; avoid GPIO/IRQ/WAKE conflicts.
    pm1.gpioSetFunc(M5PM1_GPIO_NUM_0, M5PM1_GPIO_FUNC_OTHER);
    // LED_EN 默认高电平使能灯带。
    // LED_EN default high level enables LEDs.
    pm1.setLedEnLevel(true);
    // 设置灯带数量（1-31）。
    // Set LED count (1-31).
    pm1.setLedCount(LED_COUNT);

    LOGI("LED count: %u", LED_COUNT);
    printDivider();
}

void loop() {
    static uint8_t offset = 0;

    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        // 计算彩虹色，并写入缓存。
        // Compute rainbow color and write to buffer.
        m5pm1_rgb_t c = wheel(static_cast<uint8_t>(i * 256 / LED_COUNT + offset));
        pm1.setLedColor(i, c);
    }
    // 刷新后才会显示到灯带。
    // Refresh is required to apply colors to LEDs.
    pm1.refreshLeds();

    offset++;
    delay(40);
}
