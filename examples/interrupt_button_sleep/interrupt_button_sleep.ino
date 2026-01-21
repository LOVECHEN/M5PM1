/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include <Arduino.h>
#include <M5PM1.h>

/*
 * 中断/按键/关机与唤醒示例。
 * Interrupt/button/sleep & wake demo.
 *
 * IRQ_GPIO=GPIO2 用于外部中断/唤醒输入；GPIO0/2 共用线，勿同时启用唤醒。
 * IRQ_GPIO=GPIO2 for external IRQ/wake; GPIO0/2 share a line, don't enable both.
 *
 * 双击电源键触发 PM1 关机（延迟5秒），10 秒后定时唤醒。
 * Double-click power button triggers PM1 shutdown (5s delay); timer wakes after 10s.
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
#ifndef PM1_ESP_IRQ_GPIO
#define PM1_ESP_IRQ_GPIO 13
#endif

volatile bool irqFlag = false;
void IRAM_ATTR pm1_irq_handler() {
    irqFlag = true;
}

static const m5pm1_gpio_num_t IRQ_GPIO = M5PM1_GPIO_NUM_1;
static const uint32_t WAKE_TIMER_SEC = 10;

static void printDivider() {
    Serial.println("--------------------------------------------------");
}

static void printWakeSource(uint8_t src) {
    LOGI("Wake source mask: 0x%02X", src);
    if (src & M5PM1_WAKE_SRC_TIM) LOGI("- TIMER");
    if (src & M5PM1_WAKE_SRC_VIN) LOGI("- VIN");
    if (src & M5PM1_WAKE_SRC_PWRBTN) LOGI("- PWR_BTN");
    if (src & M5PM1_WAKE_SRC_RSTBTN) LOGI("- RST_BTN");
    if (src & M5PM1_WAKE_SRC_CMD_RST) LOGI("- CMD_RESET");
    if (src & M5PM1_WAKE_SRC_EXT_WAKE) LOGI("- EXT_WAKE");
    if (src & M5PM1_WAKE_SRC_5VINOUT) LOGI("- 5VINOUT");
}

static void enterSleep() {
    LOGW("Prepare for shutdown with wake sources");

    // 先配置10s的定时开机
    // Configure 10s timer wake up first
    pm1.timerSet(WAKE_TIMER_SEC, M5PM1_TIM_ACTION_POWERON);

    // 触发双击后等待5s关机
    // Wait 5s before shutdown after double click
    LOGW("Wait 5s before shutdown...");
    delay(5000);

    // 关闭LED_EN灯显（将默认电平配置为低电平）
    // Turn off LED_EN indicator (by setting default level to LOW)
    pm1.setLedEnLevel(false);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    LOGW("Shutdown now. Wake by GPIO%u or %us timer", IRQ_GPIO, WAKE_TIMER_SEC);
    pm1.shutdown();
}

void setup() {
    Serial.begin(115200);
    delay(200);
    printDivider();
    LOGI("Interrupt + Button + Sleep demo start");

    m5pm1_err_t err = pm1.begin(&Wire, M5PM1_DEFAULT_ADDR, PM1_I2C_SDA, PM1_I2C_SCL, PM1_I2C_FREQ);
    if (err != M5PM1_OK) {
        LOGE("PM1 begin failed: %d", err);
        while (true) {
            delay(1000);
        }
    }

    // 读取并清除一次唤醒来源，便于调试本次启动原因。
    // Read and clear wake source once for debugging this boot reason.
    uint8_t wakeSrc = 0;
    if (pm1.getWakeSource(&wakeSrc, M5PM1_CLEAN_ONCE) == M5PM1_OK) {
        printWakeSource(wakeSrc);
    }

    // 清空timer设置，避免误触发。
    // Clear timer settings to avoid mis-trigger.
    pm1.timerClear();

    // 禁用单击复位/双击关机，避免与示例流程冲突。
    // Disable single-reset/double-off to avoid conflicts with demo flow.
    pm1.setSingleResetDisable(true);
    pm1.setDoubleOffDisable(true);

    pm1.btnSetConfig(M5PM1_BTN_TYPE_CLICK, M5PM1_BTN_DELAY_250MS);
    pm1.btnSetConfig(M5PM1_BTN_TYPE_DOUBLE, M5PM1_BTN_DELAY_250MS);
    pm1.btnSetConfig(M5PM1_BTN_TYPE_LONG, M5PM1_BTN_DELAY_500MS);

    pm1.gpioSetFunc(IRQ_GPIO, M5PM1_GPIO_FUNC_IRQ);
    pm1.gpioSetMode(IRQ_GPIO, M5PM1_GPIO_MODE_INPUT);
    pm1.gpioSetPull(IRQ_GPIO, M5PM1_GPIO_PULL_UP);

    // 设置ESP32的中断引脚，用于接收PM1的IRQ信号。
    // Setup ESP32 interrupt pin to receive IRQ signal from PM1.
    pinMode(PM1_ESP_IRQ_GPIO, INPUT_PULLUP);
    attachInterrupt(PM1_ESP_IRQ_GPIO, pm1_irq_handler, FALLING);

    // 启用 PM1 GPIO1 的中断输出（这里的Mask Disable代表允许产生中断）。
    // Enable PM1 GPIO1 IRQ output (Mask Disable means interrupt enabled).
    pm1.irqSetGpioMask(IRQ_GPIO, M5PM1_IRQ_MASK_DISABLE);

    // 启用按钮中断输出。
    // Enable button IRQ output.
    pm1.irqSetBtnMaskAll(M5PM1_IRQ_MASK_DISABLE);
    pm1.irqSetGpioMaskAll(M5PM1_IRQ_MASK_ENABLE);
    pm1.irqSetSysMaskAll(M5PM1_IRQ_MASK_ENABLE);

    printDivider();
}

void loop() {
    if (irqFlag) {
        irqFlag = false;

        uint8_t gpioIrq = 0;
        if (pm1.irqGetGpioStatus(&gpioIrq, M5PM1_CLEAN_ONCE) == M5PM1_OK) {
            if (gpioIrq != M5PM1_IRQ_GPIO_NONE) {
                LOGI("GPIO IRQ status: 0x%02X", gpioIrq);
                if (gpioIrq & M5PM1_IRQ_GPIO0) LOGI("- GPIO0 IRQ triggered");
                if (gpioIrq & M5PM1_IRQ_GPIO1) LOGI("- GPIO1 IRQ triggered");
                if (gpioIrq & M5PM1_IRQ_GPIO2) LOGI("- GPIO2 IRQ triggered");
                if (gpioIrq & M5PM1_IRQ_GPIO3) LOGI("- GPIO3 IRQ triggered");
                if (gpioIrq & M5PM1_IRQ_GPIO4) LOGI("- GPIO4 IRQ triggered");
            }
        }

        uint8_t btnIrq = 0;
        if (pm1.irqGetBtnStatus(&btnIrq, M5PM1_CLEAN_ONCE) == M5PM1_OK) {
            if (btnIrq != M5PM1_IRQ_BTN_NONE) {
                LOGI("Button IRQ status: 0x%02X", btnIrq);
                if (btnIrq & M5PM1_IRQ_BTN_CLICK) LOGI("- Single Click triggered");
                if (btnIrq & M5PM1_IRQ_BTN_DOUBLE) {
                    LOGI("- Double Click triggered");
                    enterSleep();
                }
                if (btnIrq & M5PM1_IRQ_BTN_WAKE) LOGI("- Wake Button triggered");
            }
        }
    } else if (digitalRead(PM1_ESP_IRQ_GPIO) == LOW) {
        // 存在未处理的中断信号，读取以清除（但忽略内容）
        // Unhandled IRQ signal exists, read to clear (but ignore content)
        uint8_t dummy;
        pm1.irqGetGpioStatus(&dummy, M5PM1_CLEAN_ALL);
        pm1.irqGetBtnStatus(&dummy, M5PM1_CLEAN_ALL);
        pm1.irqGetSysStatus(&dummy, M5PM1_CLEAN_ALL);
    }

    delay(10);
}
