///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// #include files:

#include <stdio.h>
#include <stdlib.h>
//
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_system.h>
#include <esp_log.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
//
#include "soc/gpio_struct.h"
//
#include "gfxfont.h"
#include "FreeSans9pt7b.h"
#include "FreeSans12pt7b.h"
//
#include "JSB_ILI9341.h"
#include "JSB_XPT2046.h"

#define LOG_TAG "main"

///////////////////////////////////////////////////////////////////////////////
// Build defines:

// #define DebugTouchScreen

///////////////////////////////////////////////////////////////////////////////
// Configuration:

#define ProductName "Emma's DT lamp!"

///////////////////////////////////////////////////////////////////////////////
// DisplaySPI:

// Device:
#define DisplaySPI_HostDevice HSPI_HOST

// Pins:
#define DisplaySPI_SCK_GPIO 32
#define DisplaySPI_MOSI_GPIO 33
#define DisplaySPI_MISO_GPIO 34

// DMA:
#define DisplaySPI_DMAChannel 1

///////////////////////////////////////////////////////////////////////////////
// Display:

// Pins:
#define Display_ResetX_GPIO 25
#define Display_CSX_GPIO 26
#define Display_D_CX_GPIO 27
#define Display_BacklightX_GPIO 16

///////////////////////////////////////////////////////////////////////////////
// TouchPanelSPI:

// Device:
#define TouchPanelSPI_HostDevice VSPI_HOST

// Pins:
#define TouchPanelSPI_SCK_GPIO 18
#define TouchPanelSPI_MOSI_GPIO 19
#define TouchPanelSPI_MISO_GPIO 35

// DMA:
#define TouchPanelSPI_DMAChannel 2

///////////////////////////////////////////////////////////////////////////////
// TouchPanel:

// Pins:
#define TouchPanel_CSX_GPIO 21

///////////////////////////////////////////////////////////////////////////////
// LEDs:
//
// Pins:
#define LED_Head_WarmWhite_GPIO 22
#define LED_Head_NaturalWhite_GPIO 23
#define LED_Head_Red_GPIO 2
#define LED_Head_Green_GPIO 4
#define LED_Head_Blue_GPIO 5
//
///////////////////////////////////////////////////////////////////////////////
// Utility functions:

float clamp_f(float Value, float MinValue, float MaxValue)
{
  if (Value < MinValue)
    Value = MinValue;
  if (Value > MaxValue)
    Value = MaxValue;
  return Value;
}

float CalculateInterpolationCoefficient(float Value, float MinValue, float MaxValue)
{
  return clamp_f((Value - MinValue) / (MaxValue - MinValue), 0.0f, 1.0f);
}

float sqr(float x)
{
  return x * x;
}

float cube(float x)
{
  return x * x * x;
}

///////////////////////////////////////////////////////////////////////////////
// UI functions:

static void DrawButton(uint16_t Left, uint16_t Top, uint16_t Width, uint16_t Height, uint16_t Color, char *pText)
{
  const GFXfont *pFont = ILI9341_SetFont(&FreeSans9pt7b);
  ILI9341_DrawBar(Left, Top, Width, Height, Color);
  uint16_t TextBackgroundColor = ILI9341_SetTextBackgroundColor(Color);
  ILI9341_DrawTextAtXY(pText, Left + Width / 2, Top + 24 /* Assumes height of 35! */, tpCentre);
  ILI9341_SetTextBackgroundColor(TextBackgroundColor);
  ILI9341_SetFont(pFont);
}

static uint8_t IsPointInButton(uint16_t X, uint16_t Y, uint16_t Left, uint16_t Top, uint16_t Width, uint16_t Height)
{
  if ((X < Left) || (X >= Left + Width))
    return 0;
  if ((Y < Top) || (Y >= Top + Height))
    return 0;

  return 1;
}

///////////////////////////////////////////////////////////////////////////////
// LED control:

typedef enum
{
  LED_None,
  LED_WarmWhite = LEDC_CHANNEL_0,
  LED_NaturalWhite = LEDC_CHANNEL_1,
  LED_Red = LEDC_CHANNEL_2,
  LED_Green = LEDC_CHANNEL_3,
  LED_Blue = LEDC_CHANNEL_4
} LED_t;

static void InitializeLEDControl()
{
  ledc_timer_config_t timer_conf = {0};
  ledc_channel_config_t ledc_conf = {0};

  timer_conf.duty_resolution = LEDC_TIMER_12_BIT;
  timer_conf.freq_hz = 5000;
  timer_conf.speed_mode = LEDC_HIGH_SPEED_MODE;
  timer_conf.timer_num = LEDC_TIMER_0;
  timer_conf.clk_cfg = LEDC_APB_CLK;
  ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

  ledc_conf.channel = LEDC_CHANNEL_0;
  ledc_conf.duty = 0;
  ledc_conf.gpio_num = LED_Head_WarmWhite_GPIO;
  ledc_conf.intr_type = LEDC_INTR_DISABLE;
  ledc_conf.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_conf.timer_sel = LEDC_TIMER_0;
  ledc_channel_config(&ledc_conf);
  //
  ledc_conf.channel = LEDC_CHANNEL_1;
  ledc_conf.duty = 0;
  ledc_conf.gpio_num = LED_Head_NaturalWhite_GPIO;
  ledc_conf.intr_type = LEDC_INTR_DISABLE;
  ledc_conf.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_conf.timer_sel = LEDC_TIMER_0;
  ledc_channel_config(&ledc_conf);
  //
  ledc_conf.channel = LEDC_CHANNEL_2;
  ledc_conf.duty = 0;
  ledc_conf.gpio_num = LED_Head_Red_GPIO;
  ledc_conf.intr_type = LEDC_INTR_DISABLE;
  ledc_conf.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_conf.timer_sel = LEDC_TIMER_0;
  ledc_channel_config(&ledc_conf);
  //
  ledc_conf.channel = LEDC_CHANNEL_3;
  ledc_conf.duty = 0;
  ledc_conf.gpio_num = LED_Head_Green_GPIO;
  ledc_conf.intr_type = LEDC_INTR_DISABLE;
  ledc_conf.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_conf.timer_sel = LEDC_TIMER_0;
  ledc_channel_config(&ledc_conf);
  //
  ledc_conf.channel = LEDC_CHANNEL_4;
  ledc_conf.duty = 0;
  ledc_conf.gpio_num = LED_Head_Blue_GPIO;
  ledc_conf.intr_type = LEDC_INTR_DISABLE;
  ledc_conf.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_conf.timer_sel = LEDC_TIMER_0;
  ledc_channel_config(&ledc_conf);
}

static void SetLEDBrightness(LED_t LED, float Brightness)
{
  float Drive;

  Brightness = clamp_f(Brightness, 0.0f, 1.0f);

  Drive = cube(Brightness); // Obtain good brightness control at low brightness values.

// For debugging:
//  char S[64];
//  if (LED == LED_WarmWhite)
//  {
//    sprintf(S, "Brightness Drive: %f %d           ", Brightness, (int)(4096 * Drive));
//    ILI9341_DrawTextAtXY(S, 0, 160, tpLeft);
//  }

  ledc_set_duty(LEDC_HIGH_SPEED_MODE, LED, 4096 * Drive);
  ledc_update_duty(LEDC_HIGH_SPEED_MODE, LED);
}

///////////////////////////////////////////////////////////////////////////////
// UI:

typedef enum
{
  pbNone,
  pbWhites,
  pbRed,
  pbGreen,
  pbBlue
} PressedButton_t;

// UI state:
static int Off = 0;
//
static int ButtonPressed = 0;
static PressedButton_t PressedButton = pbNone;
//
static float WarmBrightness, NaturalBrightness;
static float RedBrightness, GreenBrightness, BlueBrightness;

#define Button_White_Left 0
#define Button_White_Top 285
#define Button_White_Width 60
#define Button_White_Height 35
#define Button_White_Color ILI9341_COLOR_PURPLE
#define Button_White_Text (char *)("White")

#define Button_Off_Left 90
#define Button_Off_Top 285
#define Button_Off_Width 60
#define Button_Off_Height 35
#define Button_Off_Color ILI9341_COLOR_PURPLE
#define Button_Off_Text (char *)("Off")

#define Button_Color_Left 180
#define Button_Color_Top 285
#define Button_Color_Width 60
#define Button_Color_Height 35
#define Button_Color_Color ILI9341_COLOR_PURPLE
#define Button_Color_Text (char *)("Colour")

#define Button_Whites_Left 10
#define Button_Whites_Top 80
#define Button_Whites_Width 220
#define Button_Whites_Height 190
#define Button_Whites_Color ILI9341_COLOR_DARKGREY
#define Button_Whites_Text (char *)("")

#define Button_Red_Left 10
#define Button_Red_Top 100
#define Button_Red_Width 220
#define Button_Red_Height 35
#define Button_Red_Color ILI9341_COLOR_RED
#define Button_Red_Text (char *)("Red")

#define Button_Green_Left 10
#define Button_Green_Top 160
#define Button_Green_Width 220
#define Button_Green_Height 35
#define Button_Green_Color ILI9341_COLOR_GREEN
#define Button_Green_Text (char *)("Green")

#define Button_Blue_Left 10
#define Button_Blue_Top 220
#define Button_Blue_Width 220
#define Button_Blue_Height 35
#define Button_Blue_Color ILI9341_COLOR_BLUE
#define Button_Blue_Text (char *)("Blue")

typedef enum
{
  mdNone,
  mdWhites,
  mdColor
} Mode_t;

static Mode_t Mode = mdNone;

static void DrawScreen()
{
  ILI9341_SetFont(&FreeSans12pt7b);
  ILI9341_DrawTextAtXY(ProductName, 0, 30, tpLeft);
  ILI9341_DrawTextAtXY("Hello Emma!", 0, 65, tpLeft);

  switch (Mode)
  {
    case mdWhites:
      DrawButton(Button_Whites_Left, Button_Whites_Top, Button_Whites_Width, Button_Whites_Height, Button_Whites_Color, Button_Whites_Text);
      break;

    case mdColor:
      DrawButton(Button_Whites_Left, Button_Whites_Top, Button_Whites_Width, Button_Whites_Height, ILI9341_COLOR_BLACK, 0);

      ILI9341_DrawBar(0, Button_Red_Top, ILI9341_Width, Button_Blue_Top + Button_Blue_Height, ILI9341_COLOR_BLACK);
      DrawButton(Button_Red_Left, Button_Red_Top, Button_Red_Width, Button_Red_Height, Button_Red_Color, Button_Red_Text);
      DrawButton(Button_Green_Left, Button_Green_Top, Button_Green_Width, Button_Green_Height, Button_Green_Color, Button_Green_Text);
      DrawButton(Button_Blue_Left, Button_Blue_Top, Button_Blue_Width, Button_Blue_Height, Button_Blue_Color, Button_Blue_Text);
      break;

    default:
      break;
  }

  DrawButton(Button_White_Left, Button_White_Top, Button_White_Width, Button_White_Height, Button_White_Color, Button_White_Text);
  DrawButton(Button_Off_Left, Button_Off_Top, Button_Off_Width, Button_Off_Height, Button_Off_Color, Button_Off_Text);
  DrawButton(Button_Color_Left, Button_Color_Top, Button_Color_Width, Button_Color_Height, Button_Color_Color, Button_Color_Text);
}

static void SetMode(Mode_t Value)
{
  if (Value == Mode)
    return;

  Mode = Value;

  DrawScreen();
}

///////////////////////////////////////////////////////////////////////////////

void ProcessTouch(int16_t Touch_X, int16_t Touch_Y)
{
  if (!ButtonPressed)
  {
    PressedButton = pbNone; // Default.

    if (Off) // Turn on.
    {
      ButtonPressed = 1;
      Off = 0;
      gpio_set_level(Display_BacklightX_GPIO, 1);
      DrawScreen();
      return;
    }

    if (IsPointInButton(Touch_X, Touch_Y, Button_White_Left, Button_White_Top, Button_White_Width, Button_White_Height))
    {
      ButtonPressed = 1;
      SetMode(mdWhites);
    }
    else if (IsPointInButton(Touch_X, Touch_Y, Button_Off_Left, Button_Off_Top, Button_Off_Width, Button_Off_Height))
    {
      ButtonPressed = 1;
      ILI9341_Clear(ILI9341_COLOR_BLACK);
      gpio_set_level(Display_BacklightX_GPIO, 0);
      Off = 1;
    }
    else if (IsPointInButton(Touch_X, Touch_Y, Button_Color_Left, Button_Color_Top, Button_Color_Width, Button_Color_Height))
    {
      ButtonPressed = 1;
      SetMode(mdColor);
    }
    else if (Mode == mdWhites)
    {
      ButtonPressed = 1;
      PressedButton = pbWhites;
    }
    else if (Mode == mdColor)
    {
      if (IsPointInButton(Touch_X, Touch_Y, Button_Red_Left, Button_Red_Top, Button_Red_Width, Button_Red_Height))
      {
        ButtonPressed = 1;
        PressedButton = pbRed;
      }
      else if (IsPointInButton(Touch_X, Touch_Y, Button_Green_Left, Button_Green_Top, Button_Green_Width, Button_Green_Height))
      {
        ButtonPressed = 1;
        PressedButton = pbGreen;
      }
      else if (IsPointInButton(Touch_X, Touch_Y, Button_Blue_Left, Button_Blue_Top, Button_Blue_Width, Button_Blue_Height))
      {
        ButtonPressed = 1;
        PressedButton = pbBlue;
      }
    }
  }

  if (ButtonPressed)
  {
    switch (PressedButton)
    {
      case pbWhites:
        WarmBrightness = CalculateInterpolationCoefficient(Touch_X, Button_Whites_Left, Button_Whites_Left + Button_Whites_Width);
        NaturalBrightness = 1.0f - CalculateInterpolationCoefficient(Touch_Y, Button_Whites_Top, Button_Whites_Top + Button_Whites_Height);
        break;

      case pbRed:
        RedBrightness = CalculateInterpolationCoefficient(Touch_X, Button_Red_Left, Button_Red_Left + Button_Red_Width);
        break;

      case pbGreen:
        GreenBrightness = CalculateInterpolationCoefficient(Touch_X, Button_Green_Left, Button_Green_Left + Button_Green_Width);
        break;

      case pbBlue:
        BlueBrightness = CalculateInterpolationCoefficient(Touch_X, Button_Blue_Left, Button_Blue_Left + Button_Blue_Width);
        break;

      default:
        break;
    }
  }
}

static void Go()
{
  int16_t Touch_RawX, Touch_RawY, Touch_RawZ;
  int16_t Touch_X, Touch_Y;

  ESP_LOGV(LOG_TAG, "Go: Begin");

  ILI9341_Clear(ILI9341_COLOR_BLACK);

  WarmBrightness = 0.0f;
  NaturalBrightness = 0.0f;
  RedBrightness = 0.0f;
  GreenBrightness = 0.0f;
  BlueBrightness = 0.0f;

  SetMode(mdWhites);

  ILI9341_SetFont(&FreeSans9pt7b);
  ILI9341_SetTextDrawMode(tdmAnyCharBar);

  ESP_LOGV(LOG_TAG, "Go: Eternal loop: Begin");

  while (1)
  {
    if (XPT2046_Sample(&Touch_RawX, &Touch_RawY, &Touch_RawZ))
    {
      XPT2046_ConvertRawToScreen(Touch_RawX, Touch_RawY, &Touch_X, &Touch_Y);

#ifdef DebugTouchScreen
      ILI9341_SetFont(&FreeSans9pt7b);

      char S[64];
      sprintf(S, "Raw XYZ: %d %d %d           ", Touch_RawX, Touch_RawY, Touch_RawZ);
      ILI9341_DrawTextAtXY(S, 0, 140, tpLeft);

      sprintf(S, "XY: %d %d           ", Touch_X, Touch_Y);
      ILI9341_DrawTextAtXY(S, 0, 200, tpLeft);
#endif

      ESP_LOGV(LOG_TAG, "Go: ProcessTouch: Begin");
	  ProcessTouch(Touch_X, Touch_Y);
	  ESP_LOGV(LOG_TAG, "Go: ProcessTouch: End");
    }
    else
    {
      ButtonPressed = 0;
    }

    if (Off)
    {
      SetLEDBrightness(LED_WarmWhite, 0.0f);
      SetLEDBrightness(LED_NaturalWhite, 0.0f);
      SetLEDBrightness(LED_Red, 0.0f);
      SetLEDBrightness(LED_Green, 0.0f);
      SetLEDBrightness(LED_Blue, 0.0f);
    }
    else
    {
      SetLEDBrightness(LED_WarmWhite, WarmBrightness);
      SetLEDBrightness(LED_NaturalWhite, NaturalBrightness);
      SetLEDBrightness(LED_Red, RedBrightness);
      SetLEDBrightness(LED_Green, GreenBrightness);
      SetLEDBrightness(LED_Blue, BlueBrightness);
    }

    vTaskDelay(10 / portTICK_PERIOD_MS); // Feed watchdog.
  }
}

void app_main()
{
  esp_err_t ret;

  // Initialize the SPI buses:
  ESP_LOGI(LOG_TAG, "Initializing DisplaySPI bus...");
  fflush(stdout);
  spi_bus_config_t DisplaySPI_BusConfiguration =
  {
    .sclk_io_num = DisplaySPI_SCK_GPIO,
    .mosi_io_num = DisplaySPI_MOSI_GPIO,
    .miso_io_num = DisplaySPI_MISO_GPIO,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1
  };
  ret = spi_bus_initialize(DisplaySPI_HostDevice, &DisplaySPI_BusConfiguration, DisplaySPI_DMAChannel);
  assert(ret==ESP_OK);
  ESP_LOGI(LOG_TAG, "Done");
  fflush(stdout);
  //
  ESP_LOGI(LOG_TAG, "Initializing TouchPanelSPI bus...");
  fflush(stdout);
  spi_bus_config_t TouchPanelSPI_BusConfiguration =
  {
    .sclk_io_num = TouchPanelSPI_SCK_GPIO,
    .mosi_io_num = TouchPanelSPI_MOSI_GPIO,
    .miso_io_num = TouchPanelSPI_MISO_GPIO,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1
  };
  ret = spi_bus_initialize(TouchPanelSPI_HostDevice, &TouchPanelSPI_BusConfiguration, TouchPanelSPI_DMAChannel);
  assert(ret==ESP_OK);
  ESP_LOGI(LOG_TAG, "Done");
  fflush(stdout);

  // Set touch calibration: [Display area only! Don't include thick black bar at bottom, for example!]
  // Emma's DT lamp:
  XPT2046_RawX_Min = 220; // Was 320
  XPT2046_RawX_Max = 3800; // Was 3600
  XPT2046_RawY_Min = 250; // Was 320
  XPT2046_RawY_Max = 3700; // Was 3750

  ESP_LOGI(LOG_TAG, "Initializing Display device...");
  fflush(stdout);
  ILI9341_Initialize(DisplaySPI_HostDevice, Display_ResetX_GPIO, Display_CSX_GPIO, Display_D_CX_GPIO, Display_BacklightX_GPIO);
  ESP_LOGI(LOG_TAG, "Done");
  fflush(stdout);

  ESP_LOGI(LOG_TAG, "Initializing TouchPanel device...");
  fflush(stdout);
  XPT2046_Initialize(TouchPanelSPI_HostDevice, TouchPanel_CSX_GPIO);
  ESP_LOGI(LOG_TAG, "Done");
  fflush(stdout);

  ESP_LOGI(LOG_TAG, "Initializing LED control...");
  fflush(stdout);
  InitializeLEDControl();
  ESP_LOGI(LOG_TAG, "Done");
  fflush(stdout);

  Go();
}
