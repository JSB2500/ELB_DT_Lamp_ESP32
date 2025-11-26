///////////////////////////////////////////////////////////////////////////////
// #include files:

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
//
#include <esp_system.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
//
#include <lwip/sockets.h>
#include <sys/errno.h>
//
#include <nvs_flash.h>
//
#include "hal/spi_types.h"
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
//
#include "sdkconfig.h"
//
#include <string>
#include <regex>

static const char DefaultLogTag[] = "";
static const char WiFiLogTag[] = "WiFi";

///////////////////////////////////////////////////////////////////////////////
// Build defines:

// #define DebugTouchScreen

///////////////////////////////////////////////////////////////////////////////
// Configuration:

#include "../../WiFiCredentials.h"

#define ProductName "Emma's DT lamp!"

// WiFi: (See "C:\Espressif\frameworks\esp-idf-v5.2.2\examples\wifi\getting_started\station\main.c")
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#define EXAMPLE_H2E_IDENTIFIER ""

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
// TouchPanel pins:

#define TouchPanel_CSX_GPIO 21
///////////////////////////////////////////////////////////////////////////////
// LED pins:

#define LED_Head_WarmWhite_GPIO 22
#define LED_Head_NaturalWhite_GPIO 23
#define LED_Head_Red_GPIO 2
#define LED_Head_Green_GPIO 4
#define LED_Head_Blue_GPIO 5
///////////////////////////////////////////////////////////////////////////////
// Lamp state:

static int Off = 0;
static float WarmBrightness = 0, NaturalBrightness = 0;
static float RedBrightness = 0, GreenBrightness = 0, BlueBrightness = 0;
static uint8_t OffChanged = 0;
///////////////////////////////////////////////////////////////////////////////
// Utility functions:

static float clamp_f(float Value, float MinValue, float MaxValue)
{
  if (Value < MinValue)
    Value = MinValue;
  if (Value > MaxValue)
    Value = MaxValue;
  return Value;
}

static float CalculateInterpolationCoefficient(float Value, float MinValue, float MaxValue)
{
  return clamp_f((Value - MinValue) / (MaxValue - MinValue), 0.0f, 1.0f);
}

//static float sqr(float x)
//{
//  return x * x;
//}

static float cube(float x)
{
  return x * x * x;
}

static char Yes[] = "Yes";
static char No[] = "No";

static char *BooleanToNoYes(uint8_t Value)
{
  if (Value)
    return Yes;
  return No;
}

///////////////////////////////////////////////////////////////////////////////

void NVS_Initialize()
{
  esp_err_t ret;

  ret = nvs_flash_init();

  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }

  ESP_ERROR_CHECK(ret);
}

///////////////////////////////////////////////////////////////////////////////
// WiFi:

#define WiFi_PortNumber (80)

typedef struct 
{
    std::string ssid;
    std::string password;
} wifi_credential_t;

static std::vector<wifi_credential_t> WiFiCredentials;

static uint32_t WiFi_CurrentCredentialIndex = 0;
static EventGroupHandle_t WiFi_EventGroup;
const int WiFi_ConnectedBit = BIT0;

void  ConfigureWiFi(wifi_credential_t *pCredential)
{
  wifi_config_t wifi_config;

  ESP_LOGI(WiFiLogTag, "Configuring WiFi for SSID: %s", pCredential->ssid.c_str());

  memset(&wifi_config, 0, sizeof(wifi_config));

  strlcpy((char *)wifi_config.sta.ssid, pCredential->ssid.c_str(), sizeof(wifi_config.sta.ssid));
  strlcpy((char *)wifi_config.sta.password, pCredential->password.c_str(), sizeof(wifi_config.sta.password));
  
	/* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
	 * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
	 * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
	 * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
	 */

	wifi_config.sta.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
	wifi_config.sta.sae_pwe_h2e = ESP_WIFI_SAE_MODE,
  strlcpy((char *) wifi_config.sta.sae_h2e_identifier, EXAMPLE_H2E_IDENTIFIER, sizeof(EXAMPLE_H2E_IDENTIFIER));

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
}

static void WiFi_EventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    ESP_LOGI(WiFiLogTag, "Event: WIFI_EVENT_STA_START");
    esp_wifi_connect();
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    ESP_LOGI(WiFiLogTag, "Event: WIFI_EVENT_STA_DISCONNECTED");
    
    ++WiFi_CurrentCredentialIndex;
    if (WiFi_CurrentCredentialIndex == WiFiCredentials.size())
      WiFi_CurrentCredentialIndex = 0;
    ConfigureWiFi(&WiFiCredentials[WiFi_CurrentCredentialIndex]);
    esp_wifi_connect();
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ESP_LOGI(WiFiLogTag, "Event: IP_EVENT_STA_GOT_IP");

    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(WiFiLogTag, "Obtained IP:" IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(WiFi_EventGroup, WiFi_ConnectedBit);
  }
}

void WiFi_Initialize(void)
{
  WiFiCredentials = std::vector<wifi_credential_t>User_WiFiCredentials;
  
  WiFi_EventGroup = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta(); // JSB 20240709: Added for esp-idf V5.2.2. Without this, the IP event never happens.

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFi_EventHandler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WiFi_EventHandler, NULL, &instance_got_ip));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  WiFi_CurrentCredentialIndex = 0;
  ConfigureWiFi(&WiFiCredentials[WiFi_CurrentCredentialIndex]);
  ESP_ERROR_CHECK(esp_wifi_start());
}

void WifiServer_Go(void *)
{
  int rc;

  struct sockaddr_in clientAddress;
  struct sockaddr_in serverAddress;

  uint32_t InputBuffer_SizeInBytes = 1024;
  char *pInputBuffer = (char *)malloc(InputBuffer_SizeInBytes);
  uint32_t OutputBuffer_SizeInBytes = 1024;
  char *pOutputBuffer = (char *)malloc(OutputBuffer_SizeInBytes);

  // Create a listening socket.
  ESP_LOGI(WiFiLogTag, "Creating socket");
  int ListeningSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (ListeningSocket < 0)
  {
    ESP_LOGE(WiFiLogTag, "socket(): %d %s", ListeningSocket, strerror(errno));
    goto END;
  }

  // Bind this socket to a port.
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
  serverAddress.sin_port = htons(WiFi_PortNumber);
  ESP_LOGI(WiFiLogTag, "Binding socket");
  rc = bind(ListeningSocket, (struct sockaddr * ) &serverAddress, sizeof(serverAddress));
  if (rc < 0)
  {
    ESP_LOGE(WiFiLogTag, "bind(): %d %s", rc, strerror(errno));
    goto END;
  }

  // Listen.
  ESP_LOGI(WiFiLogTag, "Listening");
  rc = listen(ListeningSocket, 5);
  if (rc < 0)
  {
    ESP_LOGE(WiFiLogTag, "listen(): %d %s", rc, strerror(errno));
    goto END;
  }

  while (1)
  {
    ESP_LOGI(WiFiLogTag, "esp_get_free_heap_size(): %lu", esp_get_free_heap_size());
    ESP_LOGI(WiFiLogTag, "esp_get_minimum_free_heap_size(): %lu", esp_get_minimum_free_heap_size());

    // Wait for a new client connection.
    socklen_t clientAddressLength = sizeof(clientAddress);
    int ClientSocket = accept(ListeningSocket, (struct sockaddr *)&clientAddress, &clientAddressLength);
    if (ClientSocket < 0)
    {
      ESP_LOGE(WiFiLogTag, "Accept: %d %s", ClientSocket, strerror(errno));
      continue;
    }

    ESP_LOGI(WiFiLogTag, "Client connection accepted");

    int InputBuffer_NumBytes = 0;

    std::string CommandErrorMessage;

    ssize_t NumBytesRead = recv(ClientSocket, pInputBuffer, InputBuffer_SizeInBytes - 1, MSG_WAITALL);
    if (NumBytesRead <= 0) // Connection broken or error condition.
      break;
    InputBuffer_NumBytes = NumBytesRead;
    pInputBuffer[InputBuffer_NumBytes] = '\0'; // Ensure string is terminated.

    uint32_t InputBufferIndex = 0;
    std::string Line;
    uint8_t IsFirstLine = 1;

#define MaxLineNumCharsSupported (64) // Not too long or else regex_search can use vast amounts of stack.

    while (1)
    {
      if (InputBufferIndex >= InputBuffer_NumBytes)
        break;

      Line.clear();
      char *pLineStart = &pInputBuffer[InputBufferIndex];
      char *pLineEnd = strpbrk(pLineStart, "\r");
      if (!pLineEnd)
        break;
      uint32_t LineNumChars = pLineEnd - pLineStart; // Don't include terminator character.
      Line.append(pLineStart, LineNumChars > MaxLineNumCharsSupported ? MaxLineNumCharsSupported : LineNumChars);
      InputBufferIndex += LineNumChars + 2; // Skip \r\n. Ugly!

      if (!Line.size())
      {
        snprintf(pOutputBuffer, OutputBuffer_SizeInBytes, "HTTP/1.1 200 OK\r\nContent-type:text/html\r\n\r\n");
        send(ClientSocket, pOutputBuffer, strlen(pOutputBuffer), 0);

        char *pOnAsYesNo = BooleanToNoYes(!Off);
        snprintf(pOutputBuffer, OutputBuffer_SizeInBytes, "<html><body>%s<br>On: %s<br>Natural brightness: %0.2f<br>Warm brightness: %0.2f<br>Red brightness: %0.2f<br>Green brightness: %0.2f<br>Blue brightness: %0.2f</body></html>\r\n", ProductName, pOnAsYesNo, NaturalBrightness, WarmBrightness, RedBrightness, GreenBrightness, BlueBrightness);
        send(ClientSocket, pOutputBuffer, strlen(pOutputBuffer), 0);

        if (!CommandErrorMessage.empty())
        {
          snprintf(pOutputBuffer, OutputBuffer_SizeInBytes, "<BR>Command error message: %s", CommandErrorMessage.c_str());
          send(ClientSocket, pOutputBuffer, strlen(pOutputBuffer), 0);
        }

        snprintf(pOutputBuffer, OutputBuffer_SizeInBytes, "\r\n");
        send(ClientSocket, pOutputBuffer, strlen(pOutputBuffer), 0);

        break;
      }

      ESP_LOGI(WiFiLogTag, "Line: %s", Line.c_str());

      // Handle requests ("request methods") e.g. GET. See: https://en.wikipedia.org/wiki/Hypertext_Transfer_Protocol for more information.

      if (IsFirstLine) // Only check first line of request.
      {
        CommandErrorMessage = "Invalid command.";

        std::smatch SearchResults;

        if (regex_search(Line, SearchResults, std::regex("^GET /(\\S+)", std::regex_constants::icase))) // Causes stack overflow with 257 byte input string, even with 32768 byte stack!
        {
          if (SearchResults.size() > 1)
          {
            std::string Command(SearchResults.str(1));
            ESP_LOGI(WiFiLogTag, "Command: SearchResults: %s", Command.c_str());

            if (regex_search(Command, SearchResults, std::regex("^State\\?(\\S+?)=(\\S+)$", std::regex_constants::icase)))
            {
              if (SearchResults.size() > 1)
              {
                std::string ParameterName(SearchResults.str(1));
                std::string strParameterValue (SearchResults.str(2));

                float ParameterValue;
                if (sscanf(strParameterValue.c_str(), "%f", &ParameterValue))
                {
                  if (ParameterValue < 0.0f)
                    ParameterValue = 0.0f;
                  else if (ParameterValue > 1.0f)
                    ParameterValue = 1.0f;

                  // NB: Shared variables are not protected from simultaneous access!
                  uint8_t ValidCommand = 1;
                  //
                  if ((strcasecmp(ParameterName.c_str(), "N") == 0) || (strcasecmp(ParameterName.c_str(), "NaturalBrightness") == 0))
                    NaturalBrightness = ParameterValue;
                  else if ((strcasecmp(ParameterName.c_str(), "W") == 0) || (strcasecmp(ParameterName.c_str(), "WarmBrightness") == 0))
                    WarmBrightness = ParameterValue;
                  else if ((strcasecmp(ParameterName.c_str(), "R") == 0) || (strcasecmp(ParameterName.c_str(), "RedBrightness") == 0))
                    RedBrightness = ParameterValue;
                  else if ((strcasecmp(ParameterName.c_str(), "G") == 0) || (strcasecmp(ParameterName.c_str(), "GreenBrightness") == 0))
                    GreenBrightness = ParameterValue;
                  else if ((strcasecmp(ParameterName.c_str(), "B") == 0) || (strcasecmp(ParameterName.c_str(), "BlueBrightness") == 0))
                    BlueBrightness = ParameterValue;
                  else
                    ValidCommand = 0;
                  //
                  if (ValidCommand)
                    CommandErrorMessage = "";
                }
              }
            }
            else
            {
              uint8_t ValidCommand = 1;
              //
              if (strcasecmp(Command.c_str(), "Off") == 0)
              {
                Off = 1;
                OffChanged = 1;
              }
              else if (strcasecmp(Command.c_str(), "On") == 0)
              {
                Off = 0;
                OffChanged = 1;
              }
              else if (strcasecmp(Command.c_str(), "Night") == 0)
              {
               NaturalBrightness = 0.0f;
               WarmBrightness = 0.3f;
              }
              else if (strcasecmp(Command.c_str(), "Bright") == 0)
              {
               NaturalBrightness = 1.0f;
               WarmBrightness = 1.0f;
               Off = 0;
               OffChanged = 1;
              }
              else
                ValidCommand = 0; // Non-existent command.
              //
              if (ValidCommand)
                CommandErrorMessage = "";
            }
          }
        }
        else
          CommandErrorMessage = ""; // No command is OK.
      }

      IsFirstLine = 0;
    }

    shutdown(ClientSocket, 2); //??? I want the equivalent of Arduino Client.Stop().
    closesocket(ClientSocket);
  }

  free(pInputBuffer);
  free(pOutputBuffer);

END:
  vTaskDelete(NULL);
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
  ledc_timer_config_t timer_conf = {};
  ledc_channel_config_t ledc_conf = {};

  timer_conf.duty_resolution = LEDC_TIMER_12_BIT;
  timer_conf.freq_hz = 5000;
  timer_conf.speed_mode = LEDC_HIGH_SPEED_MODE;
  timer_conf.timer_num = LEDC_TIMER_0;
  timer_conf.clk_cfg = LEDC_USE_APB_CLK;
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

  ledc_set_duty(LEDC_HIGH_SPEED_MODE, ledc_channel_t(LED), 4096 * Drive);
  ledc_update_duty(LEDC_HIGH_SPEED_MODE, ledc_channel_t(LED));
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
static int ButtonPressed = 0;
static PressedButton_t PressedButton = pbNone;

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
      OffChanged = 1;
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

  ILI9341_Clear(ILI9341_COLOR_BLACK);

  WarmBrightness = 0.0f;
  NaturalBrightness = 0.0f;
  RedBrightness = 0.0f;
  GreenBrightness = 0.0f;
  BlueBrightness = 0.0f;

  Off = 0;
  OffChanged = 1;

  SetMode(mdWhites);

  ILI9341_SetFont(&FreeSans9pt7b);
  ILI9341_SetTextDrawMode(tdmAnyCharBar);

  while (1)
  {
    if (XPT2046_Sample(&Touch_RawX, &Touch_RawY, &Touch_RawZ))
    {
      XPT2046_ConvertRawToScreen(Touch_RawX, Touch_RawY, &Touch_X, &Touch_Y);

#ifdef DebugTouchScreen
        ILI9341_SetFont(&FreeSans9pt7b);
        
        char S[64];
        sESP_LOGI(DefaultLogTag, S, "Raw XYZ: %d %d %d           ", Touch_RawX, Touch_RawY, Touch_RawZ);
        ILI9341_DrawTextAtXY(S, 0, 140, tpLeft);

        sESP_LOGI(DefaultLogTag, S, "XY: %d %d           ", Touch_X, Touch_Y);
        ILI9341_DrawTextAtXY(S, 0, 200, tpLeft);
#endif

      ProcessTouch(Touch_X, Touch_Y);
    }
    else
    {
      ButtonPressed = 0;
    }

    if (OffChanged)
    {
      OffChanged = 0;
      DrawScreen();
    }

    if (Off)
    {
      gpio_set_level(gpio_num_t(Display_BacklightX_GPIO), 0);
      SetLEDBrightness(LED_WarmWhite, 0.0f);
      SetLEDBrightness(LED_NaturalWhite, 0.0f);
      SetLEDBrightness(LED_Red, 0.0f);
      SetLEDBrightness(LED_Green, 0.0f);
      SetLEDBrightness(LED_Blue, 0.0f);
    }
    else
    {
      gpio_set_level(gpio_num_t(Display_BacklightX_GPIO), 1);
      SetLEDBrightness(LED_WarmWhite, WarmBrightness);
      SetLEDBrightness(LED_NaturalWhite, NaturalBrightness);
      SetLEDBrightness(LED_Red, RedBrightness);
      SetLEDBrightness(LED_Green, GreenBrightness);
      SetLEDBrightness(LED_Blue, BlueBrightness);
    }

    vTaskDelay(10 / portTICK_PERIOD_MS); // Feed watchdog.
  }
}

extern "C"
{
  void app_main();
}

void app_main()
{
  esp_err_t ret;

  ESP_LOGI(DefaultLogTag, "Initializing NVS:");
  NVS_Initialize();
  ESP_LOGI(DefaultLogTag, "Done");

  // Initialize the SPI buses:
  ESP_LOGI(DefaultLogTag, "Initializing DisplaySPI bus:");
  spi_bus_config_t DisplaySPI_BusConfiguration =
  {
	  .mosi_io_num = DisplaySPI_MOSI_GPIO,
	  .miso_io_num = DisplaySPI_MISO_GPIO,
	  .sclk_io_num = DisplaySPI_SCK_GPIO,
	  .quadwp_io_num = -1,
	  .quadhd_io_num = -1,
	  .data4_io_num = -1,
	  .data5_io_num = -1,
	  .data6_io_num = -1,
	  .data7_io_num = -1,
    .data_io_default_level = 0,
	  .max_transfer_sz = 0,
	  .flags = 0,
      .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
	  .intr_flags = 0
  };
  ret = spi_bus_initialize(DisplaySPI_HostDevice, &DisplaySPI_BusConfiguration, DisplaySPI_DMAChannel);
  assert(ret==ESP_OK);
  ESP_LOGI(DefaultLogTag, "Done");
  //
  ESP_LOGI(DefaultLogTag, "Initializing TouchPanelSPI bus:");
  spi_bus_config_t TouchPanelSPI_BusConfiguration =
  {
    .mosi_io_num = TouchPanelSPI_MOSI_GPIO,
    .miso_io_num = TouchPanelSPI_MISO_GPIO,
    .sclk_io_num = TouchPanelSPI_SCK_GPIO,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .data4_io_num = -1,
    .data5_io_num = -1,
    .data6_io_num = -1,
    .data7_io_num = -1,
    .data_io_default_level = 0,
    .max_transfer_sz = 0,
    .flags = 0,
    .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
    .intr_flags = 0
  };
  ret = spi_bus_initialize(TouchPanelSPI_HostDevice, &TouchPanelSPI_BusConfiguration, TouchPanelSPI_DMAChannel);
  assert(ret==ESP_OK);
  ESP_LOGI(DefaultLogTag, "Done");
  fflush(stdout);

  // Set touch calibration: [Display area only! Don't include thick black bar at bottom, for example!]
  // Emma's DT lamp:
  XPT2046_RawX_Min = 220; // Was 320
  XPT2046_RawX_Max = 3800; // Was 3600
  XPT2046_RawY_Min = 250; // Was 320
  XPT2046_RawY_Max = 3700; // Was 3750

  ESP_LOGI(DefaultLogTag, "Initializing Display device:");
  ILI9341_Initialize(DisplaySPI_HostDevice, Display_ResetX_GPIO, Display_CSX_GPIO, Display_D_CX_GPIO, Display_BacklightX_GPIO);
  ESP_LOGI(DefaultLogTag, "Done");

  ESP_LOGI(DefaultLogTag, "Initializing TouchPanel device:");
  XPT2046_Initialize(TouchPanelSPI_HostDevice, TouchPanel_CSX_GPIO);
  ESP_LOGI(DefaultLogTag, "Done");

  ESP_LOGI(DefaultLogTag, "Initializing LED control:");
  InitializeLEDControl();
  ESP_LOGI(DefaultLogTag, "Done");

  ESP_LOGI(DefaultLogTag, "Initializing WiFi:");
  WiFi_Initialize();
  ESP_LOGI(DefaultLogTag, "Done");

  xTaskCreate(WifiServer_Go, "WifiServer", 32768, NULL, tskIDLE_PRIORITY, NULL); // Huge stack of 32768 due to stack usage of regex_search() for 100+ char input strings.

  Go();
}
