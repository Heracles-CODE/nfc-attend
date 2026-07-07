# NFCAttend v1.0 Phase1 — 项目文档

> STM32F407VET6 + FreeRTOS + RC522 + OLED + W25Q128 + ESP01S(N/A Phase1)
> 编译工具链: `arm-none-eabi-gcc 14.2.1` (位于 `STM32/arm-none-eabi-gcc/`)
> 烧录工具: `OpenOCD 0.12.0` (位于 `STM32/openocd/`)

---

## 一、项目概述

### 1.1 硬件平台

| 项目 | 规格 |
|---|---|
| MCU | STM32F407VET6, Cortex-M4F, 168 MHz |
| 调试口 | SWD (CMSIS-DAP) + USART1 (printf/COM15) |
| 显示 | 0.96" OLED 128×64, I2C1 (PB6/PB7), 地址 0x3C |
| 卡片 | RC522, 软件模拟 SPI (PE15 NSS / PB15 RST / PA0 MOSI / PB13 MISO / PD9 SCK / PB10 GND) |
| 存储 | W25Q128, SPI1 (PA5/6/7), 16 MB |
| 联网 | ESP01S, USART6 (PC6/PC7), **Phase1 不启用** |
| RTC | LSE 32.768 kHz + VBAT |
| 人机 | 6 按键 (PE1~PE6) + 7 LED (PE8~PE14) + 蜂鸣器 TIM3_CH1 (PB4) |

### 1.2 软件架构

```
App/Src/
  app_main.c      — 启动初始化(串口/RTC/W25Q/LED/按键),不含调度器依赖
  app_tasks.c    — 6 个 FreeRTOS 任务入口
  nfc_app.c      — RC522 寻卡/认证/读Block1/事件分发(防抖设计)
  display.c      — OLED 场景状态机(STARTUP/CLOCK/CARD_INFO/TIME_SETTING)
  key_proc.c     — 按键事件分发(时钟/时间设置两套逻辑)
  card_format.c  — Block1 编解码/校验(A5 5A 尾+异或校验)
  storage.c      — W25Q128 设备号/考勤模式持久化

Core/Src/
  freertos.c     — 6 任务注册(osThreadNew),与 ioc FREERTOS.Tasks01 对应
  main.c          — HAL_Init → MX_* → App_BeforeScheduler() → osKernelStart

program/NFCAttend/
  NFCAttend.ioc           — CubeMX 配置(STM32F407VET6,全外设已配)
  STM32Make.make           — Makefile(引用 program/Bsp/ 共享驱动)
  openocd.cfg             — CMSIS-DAP + STM32F4x
  .stm32env              — ARM_GCC_PATH / OPENOCD 绝对路径
```

### 1.3 任务划分

| 任务 | 入口 | 优先级(osPriority) | 栈(字) | 周期 | 职责 |
|---|---|---|---|---|---|
| defaultTask | StartDefaultTask | Normal(24) | 512 | 100 ms | 系统心跳/Heap 日志 |
| guiTask | StartTaskGui | Normal(8) | 1024 | 50 ms | OLED 刷新 + DISP_Init |
| keyTask | StartTaskKey | Normal(24) | 512 | 10 ms | Key_Scan + 事件分发 |
| uartTask | StartTaskUart | Normal(24) | 1024 | 100 ms | 预留(Phase2 ESP01S) |
| otherTask | StartTaskOther | Normal(9) | 512 | 20 ms | 蜂鸣器 MIDI_Tick |
| nfcTask | StartTaskNFC | Normal(8) | 512 | 100 ms | RC522 寻卡 + NFC_App_Init |

---

## 二、卡片数据格式

Mifare S50, 16 扇区 × 4 块 × 16 字节。Phase1 使用扇区 0 块 1:

```
Block 1 (16 B):
  [0..3]   CID   — Card UID (Big-endian, HEX 8位)
  [4..7]   SID   — Staff ID (Big-endian, DEC 5位)
  [8..9]   PTS   — Points (Big-endian)
  [10..11] 保留
  [12]     CTYPE — 类型: 0=未发卡 1=员工 2=管理员
  [13]     校验  — XOR[0..12]
  [14]     0xA5  — 固定尾
  [15]     0x5A  — 固定尾
```

默认出厂密钥: `FF FF FF FF FF FF`

---

## 三、OLED 显示状态机

```
STARTUP ──40×50ms=2s──▶ CLOCK ──K6长按──▶ TIME_SETTING ──K6短按──▶ CLOCK
                        │
                        └──刷卡VALID──▶ CARD_INFO(OK) ──3s无卡──▶ CLOCK
                        └──刷卡INVALID──▶ CARD_INFO(ERR)
```

### CLOCK 界面
```
YYYY-MM-DD
HH:MM:SS
DEV:1  MODE:0
```

### CARD_INFO 界面(3秒后自动回CLOCK)
```
CARD OK / CARD ERR
UID:XXXXXXXX
SID:XXXXX
TYPE:X  PTS:X
```

---

## 四、引脚定义(与 NFCAttend.ioc 一致)

```
PE1  K1 (上拉,低有效)     PE8  L1 (低亮)
PE2  K2 (上拉,低有效)     PE9  L2 (低亮)
PE3  K3 (上拉,低有效)     PE10 L3 (低亮)
PE4  K4 (上拉,低有效)     PE11 L4 (低亮)
PE5  K5 (下拉,高有效)     PE12 L5 (低亮)
PE6  K6 (下拉,高有效)     PE13 L6 (低亮)
                           PE14 L7 (低亮)
PA0  NFC_MOSI             PB4  BEEP(TIM3_CH1)
PB10 NFC_GND              PB13 NFC_MISO
PB15 NFC_RST              PC4  SPI1_CS(W25Q)
PD9  NFC_SCK              PE15 NFC_NSS
PA5  SPI1_SCK             PA6  SPI1_MISO
PA7  SPI1_MOSI            PC6  USART6_TX(ESP01S)
PC7  USART7_RX(ESP01S)    PA9  USART1_TX
PA10 USART1_RX            PB6  I2C1_SCL(OLED)
PB7  I2C1_SDA(OLED)
```

---

## 五、编译与烧录

### 5.1 编译
```bash
cd "c:/Users/Heracles/Downloads/STM32/program/NFCAttend"
export PATH="/c/Users/Heracles/Downloads/STM32/windows-build-tools/.content/bin:$PATH"
make -f STM32Make.make all        # 编译
make -f STM32Make.make clean      # 清理
```

### 5.2 烧录(接好 CMSIS-DAP 到 SWDIO/SWDCLK)
```bash
make -f STM32Make.make flash
# 等效: openocd -f openocd.cfg -c "program build/debug/NFCAttend.elf verify reset exit"
```

### 5.3 资源占用
- **FLASH**: 57 728 B (≈56.4 KB, 占 512 KB 的 11%)
- **RAM**: 43 376 B (≈42.4 KB, 占 128 KB 的 33%)

---

## 六、串口调试(COM15, 115200 8N1)

上电后串口输出:
```
=== NFCAttend Phase1 boot ===
[RTC] YYYY-MM-DD HH:MM:SS
[W25Q] ID=0xEF17
[APP] GPIO init done
[APP] Key init done
[APP] NFC init deferred to nfcTask
[APP] all pre-scheduler init done, starting scheduler...
[NFC] init start (in task)
[NFC] init done
[GUI] OLED init done
[HB] tick=4s heap=32768
[HB] tick=9s heap=32768
...
```

刷卡输出示例:
```
[NFC] OK UID=03FE4592 SID=12345 T=1
[NFC] LEAVE
```

---

## 七、操作说明

### 7.1 开机
上电后 OLED 显示 `NFC Attend v1.0 phase1` → 约2秒后自动转实时时钟界面。

### 7.2 刷卡(员工卡)
- OLED 显示 `CARD OK` + UID + SID + TYPE + PTS
- 蜂鸣器发出 C5 音符(约100ms)
- L1(PE8) 亮
- 3秒后自动返回时钟界面
- 移开卡片后打印 `[NFC] LEAVE`

### 7.3 刷卡(未发卡/无效卡)
- OLED 显示 `CARD ERR`
- 蜂鸣器发出 A4 音符(约200ms)
- L2(PE9) 亮
- 3秒后返回时钟界面

### 7.4 设置时间
1. **长按 K6**(PE6) → 进入时间设置界面
2. **K2/K3** → 调整当前字段数值(支持连发)
3. **K4** → 切换年/月/日/时/分/秒字段
4. **短按 K6** → 保存并返回时钟界面

---

## 八、已修复的 Bug (v1.0.1 patch)

| # | 问题 | 根因 | 修复 |
|---|---|---|---|
| 1 | OLED 卡在 startup 不切换 | `HAL_GetTick()` 在 `osKernelStart()` 前 SysTick 被 FreeRTOS 重配,计时不递增 | 改用 `s_startup_calls` 纯计数,约2秒后切换 |
| 2 | 系统反复重启 | `RC522_Platform_Init()` 在调度器启动前调用,RC522不存在时阻塞;`taskENTER_CRITICAL()` 在调度器未运行时行为不确定 | 全部硬件初始化推迟到各自任务首次执行 |
| 3 | RTC 时间 2000-01-01 每次上电都"首次上电" | RTC Alarm A 在 backup domain 断电后首次上电立即触发 Alarm 中断,导致复位 | `disable_rtc_alarm()` 在 RTC 初始化后立即禁用 Alarm A 并清除标志 |
| 4 | 蜂鸣器一直响,OLED 闪烁 | 卡持续在位时 INVALID 事件每100ms重复触发,`MIDI_Beep()` 叠加蜂鸣且每次都触发 `DISP_SetState` 清屏 | 加 `s_notified_invalid/valid` 标志,卡在场时不重复触发;DISP_CARD_INFO 内容不变时不重复 `GUI_Clear` |
| 5 | 卡片移开时 INVALID 快速循环 | `RC522_Halt()` 让卡休眠,但100ms后下一轮 scan 又唤醒它 | 去掉 `RC522_Halt()`,卡保持在场,同一 UID 在"卡在场"分支处理,离开后连续3次检测失败才判 LEAVE |

---

## 九、Phase2 计划

- [ ] ESP01S WiFi 连接 + NTP 校时 → RTC
- [ ] TCP 透传考勤记录到 PC 服务器
- [ ] W25Q128 考勤记录落盘 + 断网续传
- [ ] PC 端 Qt 发卡工具(ISSUE/IMGA/IMGN/IMGD/UPDATEIMG 命令)
- [ ] AES 加密通讯

---

## 十、文件索引

```
program/NFCAttend/
├── App/
│   ├── Inc/   app_tasks.h card_format.h display.h key_proc.h nfc_app.h storage.h
│   └── Src/   app_main.c app_tasks.c card_format.c display.c key_proc.c nfc_app.c storage.c
├── Core/
│   ├── Inc/   main.h FreeRTOSConfig.h ...
│   └── Src/   main.c freertos.c gpio.c rtc.c spi.c usart.c ...
├── Drivers/   STM32F4xx_HAL_Driver/  CMSIS/
├── Middlewares/  Third_Party/FreeRTOS/
├── NFCAttend.ioc
├── STM32Make.make
├── openocd.cfg
├── .stm32env
└── STM32F407XX_FLASH.ld

program/Bsp/  (共享驱动,引用自此目录,勿移动)
├── NFC/    rc522.c rc522.h rc522_platform_stm32.c
├── OLED/   ssd1306.c/h ssd1306_i2c.c/h GUISlim.c GUI.h F08_ASCII.c
├── w25qxx/ w25qxx.c w25qxx.h
├── RTC/    bsp_rtc.c bsp_rtc.h
├── Key/    key.c key.h
├── LED/    led.c led.h
├── MIDI/   midi.c midi.h
├── delay/   delay_us.c delay_us.h
└── UartDrv/ uart_drv.c uart_drv.h
```
