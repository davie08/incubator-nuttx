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

#ifdef CONFIG_INPUT_FT5X06
#include <nuttx/input/ft5x06.h>
#define FT5X06_I2C_ADDRESS 0x38
#define FT5X06_FREQUENCY  400000
#define GPIO_TOUCH_INT 38
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct twatch_ft5x06_config_s
{
  xcpt_t          handler;  /* The FT5x06 interrupt handler */
  FAR void       *arg;      /* Interrupt handler argument */
};

/****************************************************************************
 * Private Function Ptototypes
 ****************************************************************************/

#ifdef CONFIG_INPUT_FT5X06
#ifndef CONFIG_FT5X06_POLLMODE
static int  twatch_ft5x06_attach(FAR const struct ft5x06_config_s *config,
                                xcpt_t isr, FAR void *arg);
static void twatch_ft5x06_enable(FAR const struct ft5x06_config_s *config,
                                bool enable);
static void twatch_ft5x06_clear(FAR const struct ft5x06_config_s *config);
#endif
static void twatch_ft5x06_wakeup(FAR const struct ft5x06_config_s *config);
static void twatch_ft5x06_nreset(FAR const struct ft5x06_config_s *config,
                                bool state);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct lcd_dev_s *g_lcd ;

#ifdef CONFIG_INPUT_FT5X06
static struct twatch_ft5x06_config_s g_priv_config =
{
  .handler     = NULL,
  .arg         = NULL,
};

static const struct ft5x06_config_s g_ft5x06_config =
{
  .address   = FT5X06_I2C_ADDRESS,
  .frequency = FT5X06_FREQUENCY,
#ifndef CONFIG_FT5X06_POLLMODE
  .attach    = twatch_ft5x06_attach,
  .enable    = twatch_ft5x06_enable,
  .clear     = twatch_ft5x06_clear,
#endif
  .wakeup    = twatch_ft5x06_wakeup,
  .nreset    = twatch_ft5x06_nreset
};
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef CONFIG_AXP202
int board_pmu_initialize(FAR const char *devname)
{
  FAR struct battery_charger_dev_s *axp202_dev;
  FAR struct i2c_master_s *i2c;

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

#ifdef CONFIG_INPUT_FT5X06

#ifndef CONFIG_FT5X06_DEVMINOR
  #define CONFIG_FT5X06_DEVMINOR 0
#endif

#ifndef CONFIG_FT5X06_POLLMODE
static int  twatch_ft5x06_attach(FAR const struct ft5x06_config_s *config,
                                xcpt_t isr, FAR void *arg)
{
  /* Just save the handler.  We will use it when EXTI interruptsare enabled */

  if (isr)
    {
      /* Just save the address of the handler for now.  The new handler will
       * be attached when the interrupt is next enabled.
       */

      syslog(LOG_ERR, "Attaching %p\n", isr);
      g_priv_config.handler = isr;
      g_priv_config.arg     = arg;
    }
  else
    {
      syslog(LOG_ERR, "Detaching %p\n", g_priv_config.handler);
      twatch_ft5x06_enable(config, false);
      g_priv_config.handler = NULL;
      g_priv_config.arg     = NULL;
    }

  return 0;
}

static void twatch_ft5x06_enable(FAR const struct ft5x06_config_s *config,
                                bool enable)
{
  int irq = ESP32_PIN2IRQ(GPIO_TOUCH_INT);
  int ret = 0;

  if (enable)
    {
      /* Configure the EXTI interrupt using the SAVED handler */

      if (NULL != g_priv_config.handler)
        {
          /* Make sure the interrupt is disabled */

          esp32_gpioirqdisable(irq);

          ret = irq_attach(irq, g_priv_config.handler, g_priv_config.arg);
          if (ret < 0)
            {
              syslog(LOG_ERR, "ERROR: irq_attach() failed: %d\n", ret);
              return ;
            }

          /* Configure the interrupt for rising and falling edges */

          esp32_gpioirqenable(irq, CHANGE);
        }
      else
        {
          esp32_gpioirqdisable(irq);
        }
    }
  else
    {
      /* Configure the EXTI interrupt with a NULL handler to disable it */

     esp32_gpioirqdisable(irq);
    }

  return;
}

static void twatch_ft5x06_clear(FAR const struct ft5x06_config_s *config)
{
  return;
}

static void twatch_ft5x06_wakeup(FAR const struct ft5x06_config_s *config)
{
  return;
}

static void twatch_ft5x06_nreset(FAR const struct ft5x06_config_s *config,
                                bool nstate)
{
  return;
}
#endif

int board_touch_initialize(void)
{
  FAR struct i2c_master_s *i2c;

  i2c = esp32_i2cbus_initialize(1);

  if (!i2c)
    {
      return -1;
    }

  esp32_configgpio(GPIO_TOUCH_INT, INPUT_FUNCTION_3 | PULLUP);

  ft5x06_register(i2c, &g_ft5x06_config, CONFIG_FT5X06_DEVMINOR);

  return 0;
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
