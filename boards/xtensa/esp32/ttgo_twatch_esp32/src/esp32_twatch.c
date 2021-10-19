/****************************************************************************
 * boards/xtensa/esp32/ttgo_twatch_esp32/src/esp32_twatch.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <syslog.h>
#include <debug.h>
#include <stdio.h>

#include <errno.h>
#include <nuttx/fs/fs.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/power/battery_charger.h>
#include <nuttx/lcd/lcd.h>
#include <arch/board/board.h>

#include "esp32_gpio.h"

#ifdef CONFIG_ESP32_I2C
#  include "esp32_board_i2c.h"
#  include "esp32_i2c.h"
#endif

#ifdef CONFIG_SPI_DRIVER
#  include "esp32_spi.h"
#endif

#include "ttgo_twatch_esp32.h"

#ifdef CONFIG_AXP202
#include <nuttx/power/axp202.h>
#endif

#ifdef CONFIG_LCD_ST7789
#include <nuttx/lcd/st7789.h>
#endif

struct lcd_dev_s *g_lcd ;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef CONFIG_AXP202
int board_pmu_initialize(FAR const char *devname)
{
  FAR struct battery_charger_dev_s *axp202_dev;
  struct i2c_master_s *i2c;

  i2c = esp32_i2cbus_initialize(0);

  axp202_dev = (FAR struct battery_charger_dev_s *)axp202_initialize(
                                                     i2c,
                                                     AXP202_SLAVE_ADDRESS,
                                                     100000);

  if (axp202_dev)
    {
      return battery_charger_register(devname, axp202_dev);
    }

  return -1;
}
#endif

#ifdef CONFIG_LCD_ST7789
int board_lcd_initialize(void)
{
  FAR struct spi_dev_s *spi;
  spi = esp32_spibus_initialize(2);
  if (!spi)
    {
      printf("Failed to initialize SPI bus.\n");
      return -ENODEV;
    }

  printf("st7789_lcdinitialize\n");

  g_lcd = st7789_lcdinitialize(spi);
  g_lcd->setpower(g_lcd, 1);

  /* patch for st7789 */

  SPI_SELECT(spi, SPIDEV_DISPLAY(0), true);
  SPI_SETMODE(spi, CONFIG_LCD_ST7789_SPIMODE);
  SPI_SETBITS(spi, 8);
  SPI_SETFREQUENCY(spi, CONFIG_LCD_ST7789_FREQUENCY);

  SPI_CMDDATA(spi, SPIDEV_DISPLAY(0), true);
  SPI_SEND(spi, 176);
  SPI_CMDDATA(spi, SPIDEV_DISPLAY(0), false);

  SPI_SELECT(spi, SPIDEV_DISPLAY(0), false);

  SPI_SELECT(spi, SPIDEV_DISPLAY(0), true);
  SPI_SETMODE(spi, CONFIG_LCD_ST7789_SPIMODE);
  SPI_SETBITS(spi, 8);
  SPI_SETFREQUENCY(spi, CONFIG_LCD_ST7789_FREQUENCY);

  SPI_SEND(spi, 0x00);
  SPI_SEND(spi, 200);

  SPI_SELECT(spi, SPIDEV_DISPLAY(0), false);

  return 0;
}

FAR struct lcd_dev_s *board_lcd_getdev(int lcddev)
{
  if (lcddev == 0)
    {
      return g_lcd;
    }

  return NULL;
}

void board_lcd_uninitialize(void)
{
}

int board_display_initialize(void)
{
  int ret = 0;

  /* tft dc output */

  esp32_configgpio(DISPLAY_DC, OUTPUT);

  ret = fb_register(0, 0);

  if (ret < 0)
    {
      printf("ERROR: fb_register() failed: %d\n", ret);
      return -ENODEV;
    }

  /* backlight open */

  esp32_configgpio(DISPLAY_BACKLIGHT, OUTPUT);
  esp32_gpiowrite(DISPLAY_BACKLIGHT, 1);

  return 0;
}
#endif