// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2024 Scott Shawcroft for Adafruit Industries
//
// SPDX-License-Identifier: MIT

#define MICROPY_HW_BOARD_NAME "Adafruit Metro RP2350"
#define MICROPY_HW_MCU_NAME "rp2350b"

#define MICROPY_HW_NEOPIXEL (&pin_GPIO25)

#define DEFAULT_I2C_BUS_SCL (&pin_GPIO21)
#define DEFAULT_I2C_BUS_SDA (&pin_GPIO20)

#define DEFAULT_SPI_BUS_SCK (&pin_GPIO30)
#define DEFAULT_SPI_BUS_MOSI (&pin_GPIO31)
#define DEFAULT_SPI_BUS_MISO (&pin_GPIO28)

#define DEFAULT_UART_BUS_RX (&pin_GPIO1)
#define DEFAULT_UART_BUS_TX (&pin_GPIO0)

// #define CIRCUITPY_CONSOLE_UART_RX DEFAULT_UART_BUS_RX
// #define CIRCUITPY_CONSOLE_UART_TX DEFAULT_UART_BUS_TX

#define DEFAULT_USB_HOST_DATA_PLUS (&pin_GPIO32)
#define DEFAULT_USB_HOST_DATA_MINUS (&pin_GPIO33)
#define DEFAULT_USB_HOST_5V_POWER (&pin_GPIO29)
#define CIRCUITPY_PSRAM_CHIP_SELECT (&pin_GPIO47)

#define DEFAULT_DVI_BUS_CLK_DN (&pin_GPIO15)
#define DEFAULT_DVI_BUS_CLK_DP (&pin_GPIO14)
#define DEFAULT_DVI_BUS_RED_DN (&pin_GPIO19)
#define DEFAULT_DVI_BUS_RED_DP (&pin_GPIO18)
#define DEFAULT_DVI_BUS_GREEN_DN (&pin_GPIO17)
#define DEFAULT_DVI_BUS_GREEN_DP (&pin_GPIO16)
#define DEFAULT_DVI_BUS_BLUE_DN (&pin_GPIO13)
#define DEFAULT_DVI_BUS_BLUE_DP (&pin_GPIO12)

// These SD pins double as the 4-bit SDIO interface (board.SDIO_*): SCK=CLOCK,
// MOSI=COMMAND, MISO=DATA0, CS=DATA3. By default the SPI automount claims them
// and mounts the card over SPI, so sdioio.SDCard() would fail with "<pin> in
// use". Set CIRCUITPY_SDCARD_USB = false in settings.toml to free the pins for
// sdioio (this also disables the automatic /sd mount on this board).
#define DEFAULT_SD_SCK (&pin_GPIO34)
#define DEFAULT_SD_MOSI (&pin_GPIO35)
#define DEFAULT_SD_MISO (&pin_GPIO36)
#define DEFAULT_SD_CS (&pin_GPIO39)
#define DEFAULT_SD_CARD_DETECT (&pin_GPIO40)
#define DEFAULT_SD_CARD_INSERTED false
