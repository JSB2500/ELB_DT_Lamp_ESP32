///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Copyright 2017 J S Bladen.
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Acknowledgements:
//
// => ILI9341 driver based on Adafruit ILI9341 driver and ESP32 esp-idf spi_master example.
// => Font technology based on Adafruit graphics / font libraries.
///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//
#include "esp_system.h"
#include "esp_log.h"
#include "soc/gpio_struct.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
//
#include "JSB_ILI9341.h"

#define LOG_TAG "JSB_ILI9341"

///////////////////////////////////////////////////////////////////////////////

#define TextColor_Default ILI9341_COLOR_WHITE
#define TextBackgroundColor_Default ILI9341_COLOR_BLACK
#define TextDrawMode_Default tdmThisCharBar
///////////////////////////////////////////////////////////////////////////////

#define SPI_MaxNumTransactions 6

///////////////////////////////////////////////////////////////////////////////

static spi_device_handle_t spi;
static int D_CX_GPIO;
static uint16_t TextColor = TextColor_Default;
static uint16_t TextBackgroundColor = TextBackgroundColor_Default;
static TextDrawMode_t TextDrawMode = TextDrawMode_Default;
static const GFXfont *pFont = NULL;

///////////////////////////////////////////////////////////////////////////////
// ILI9341 commands, mostly from Adafruit IPI9341 library:

#define ILI9341_NOP                 0x00
#define ILI9341_SWRESET             0x01
#define ILI9341_RDDID               0x04
#define ILI9341_RDDST               0x09

#define ILI9341_RDMODE              0x0A
#define ILI9341_RDMADCTL            0x0B
#define ILI9341_RDPIXFMT            0x0C
#define ILI9341_RDIMGFMT            0x0D
#define ILI9341_RDDSM               0x0E
#define ILI9341_RDSELFDIAG          0x0F

#define ILI9341_SLPIN               0x10
#define ILI9341_SLPOUT              0x11
#define ILI9341_PTLON               0x12
#define ILI9341_NORON               0x13

#define ILI9341_INVOFF              0x20
#define ILI9341_INVON               0x21
#define ILI9341_GAMMASET            0x26
#define ILI9341_DISPOFF             0x28
#define ILI9341_DISPON              0x29

#define ILI9341_CASET               0x2A
#define ILI9341_PASET               0x2B
#define ILI9341_RAMWR               0x2C
#define ILI9341_RGBSET              0x2D
#define ILI9341_RAMRD               0x2E

#define ILI9341_PTLAR               0x30
#define ILI9341_VSCRDEF             0x33
#define ILI9341_TEOFF               0x34
#define ILI9341_TEON                0x35
#define ILI9341_MADCTL              0x36
#define ILI9341_VSCRSADD            0x37
#define ILI9341_IDMOFF              0x38
#define ILI9341_IDMON               0x39
#define ILI9341_PIXFMT              0x3A
#define ILI9341_WRITE_MEM_CONTINUE  0x3C
#define ILI9341_READ_MEM_CONTINUE   0x3E

#define ILI9341_SET_TEAR_SCANLINE   0x44
#define ILI9341_GET_SCANLINE        0x45

#define ILI9341_WDB                 0x51
#define ILI9341_RDDISBV             0x52
#define ILI9341_WCD                 0x53
#define ILI9341_RDCTRLD             0x54
#define ILI9341_WRCABC              0x55
#define ILI9341_RDCABC              0x56
#define ILI9341_WRITE_CABC          0x5E
#define ILI9341_READ_CABC           0x5F

#define ILI9341_RGB_INTERFACE       0xB0
#define ILI9341_FRMCTR1             0xB1
#define ILI9341_FRMCTR2             0xB2
#define ILI9341_FRMCTR3             0xB3
#define ILI9341_INVCTR              0xB4
#define ILI9341_BPC                 0xB5
#define ILI9341_DFUNCTR             0xB6
#define ILI9341_ETMOD               0xB7
#define ILI9341_BACKLIGHT1          0xB8
#define ILI9341_BACKLIGHT2          0xB9
#define ILI9341_BACKLIGHT3          0xBA
#define ILI9341_BACKLIGHT4          0xBB
#define ILI9341_BACKLIGHT5          0xBC
#define ILI9341_BACKLIGHT7          0xBE
#define ILI9341_BACKLIGHT8          0xBF

#define ILI9341_PWCTR1              0xC0
#define ILI9341_PWCTR2              0xC1
#define ILI9341_PWCTR3              0xC2
#define ILI9341_PWCTR4              0xC3
#define ILI9341_PWCTR5              0xC4
#define ILI9341_VMCTR1              0xC5
#define ILI9341_VMCTR2              0xC7

#define LCD_NVMWR                   0xD0   /* NV Memory Write */
#define LCD_NVMPKEY                 0xD1   /* NV Memory Protection Key */
#define LCD_RDNVM                   0xD2   /* NV Memory Status Read */
#define LCD_READ_ID4                0xD3   /* Read ID4 */

#define ILI9341_RDID1               0xDA
#define ILI9341_RDID2               0xDB
#define ILI9341_RDID3               0xDC
#define ILI9341_RDID4               0xDD

#define ILI9341_GMCTRP1             0xE0
#define ILI9341_GMCTRN1             0xE1
#define LCD_DGAMCTRL1               0xE2
#define LCD_DGAMCTRL2               0xE3
#define LCD_INTERFACE               0xF6

/* Extend register commands */
#define LCD_POWERA                  0xCB
#define LCD_POWERB                  0xCF
#define LCD_DTCA                    0xE8
#define LCD_DTCB                    0xEA
#define LCD_POWER_SEQ               0xED
#define LCD_3GAMMA_EN               0xF2
#define LCD_PRC                     0xF7
///////////////////////////////////////////////////////////////////////////////
// Utilities:

uint16_t ILI9341_SwapBytes(uint16_t Value)
{
  return (Value >> 8) | ((Value & 0xFF) << 8);
}

uint32_t min32(uint32_t A, uint32_t B)
{
  return A < B ? A : B;
}

///////////////////////////////////////////////////////////////////////////////

static void ILI9341_SendCommand(spi_device_handle_t spi, const uint8_t cmd)
// Waits until the transfer is complete.
{
  esp_err_t ret;
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));       //Zero out the transaction
  t.length = 8;                     //Command is 8 bits
  t.tx_buffer = &cmd;               //The data is the cmd itself
  t.user = (void*) 0;                //D/C needs to be set to 0
  ret = spi_device_polling_transmit(spi, &t);  //Transmit! // JSB 20240709: Was spi_device_transmit()
  assert(ret==ESP_OK);            //Should have had no issues.
}

static void ILI9341_SendData(spi_device_handle_t spi, const uint8_t *data, int len)
// Waits until the transfer is complete.
{
  esp_err_t ret;
  spi_transaction_t t;
  if (len == 0)
    return;             //no need to send anything
  memset(&t, 0, sizeof(t));       //Zero out the transaction
  t.length = len * 8;                 //Len is in bytes, transaction length is in bits.
  t.tx_buffer = data;               //Data
  t.user = (void*) 1;                //D/C needs to be set to 1
  ret = spi_device_polling_transmit(spi, &t);  //Transmit! // JSB 20240709: Was spi_device_transmit()
  assert(ret==ESP_OK);            //Should have had no issues.
}

void ILI9341_SPI_PreTransferCallback(spi_transaction_t *t)
// Set D/~C GPIO output just prior to transfer.
{
  int dc = (int) t->user;
  gpio_set_level(D_CX_GPIO, dc);
}

void ILI9341_SetDefaults()
{
  TextColor = TextColor_Default;
  TextBackgroundColor = TextBackgroundColor_Default;
  TextDrawMode = TextDrawMode_Default;
}

typedef struct
{
  uint8_t Command;
  uint8_t Data[16];
  uint8_t NumDataBytes; // Bit 7 => Delay after set. 0xFF => End of commands.
} ILI9341_InitializationCommand_t;

DRAM_ATTR static const ILI9341_InitializationCommand_t ILI9341_InitializationCommands[] =
// DRAM_ATTR => place in DRAM. By default, constant data gets placed into DROM, which is not accessible by DMA.
// Acknowledgements:
// => Idea to use a structure for each command from esp-idf spi_master example.
// => Commands from both Adafruit ILI9341 Arduino driver and esp-idf spi_master example.
{
    { LCD_POWERB, { 0x00, 0xC1, 0X30 }, 3 },
    { LCD_POWER_SEQ, { 0x64, 0x03, 0X12, 0X81 }, 4 },
    { LCD_DTCA, { 0x85, 0x00, 0x78 }, 3 },
    { LCD_POWERA, { 0x39, 0x2C, 0x00, 0x34, 0x02 }, 5 },
    { LCD_PRC, { 0x20 }, 1 },
    { LCD_DTCB, { 0x00, 0x00 }, 2 },
    { ILI9341_PWCTR1, { 0x26 }, 1 },
    { ILI9341_PWCTR2, { 0x11 }, 1 },
    { ILI9341_VMCTR1, { 0x3E, 0x28 }, 2 },
    { ILI9341_VMCTR2, { 0x86 }, 1 },
    { ILI9341_MADCTL, { 0x48 }, 1 },
    { ILI9341_VSCRSADD, { 0x00 }, 1 },
    { ILI9341_PIXFMT, { 0x55 }, 1 },
    { ILI9341_FRMCTR1, { 0x00, 0x18 }, 2 },
    { ILI9341_DFUNCTR, { 0x08, 0x82, 0x27 }, 3 },
    { LCD_3GAMMA_EN, { 0x08 }, 1 },
    { ILI9341_GAMMASET, { 0x01 }, 1 },
    { ILI9341_GMCTRP1, { 0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00 }, 15 },
    { ILI9341_GMCTRN1, { 0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F }, 15 },
    { ILI9341_CASET, { 0x00, 0x00, 0x00, 0xEF }, 4 },
    { ILI9341_PASET, { 0x00, 0x00, 0x01, 0x3F }, 4 },
    { ILI9341_RAMWR, { 0 }, 0 },
    { ILI9341_ETMOD, { 0x07 }, 1 },
    { ILI9341_SLPOUT, { 0 }, 0x80 },
    { ILI9341_DISPON, { 0 }, 0x80 },
    { 0, { 0 }, 0xff }
};

void ILI9341_Initialize(spi_host_device_t HostDevice, int i_ResetX_GPIO, int i_CSX_GPIO, int i_D_CX_GPIO, int i_BacklightX_GPIO)
{
  esp_err_t ret;

  D_CX_GPIO = i_D_CX_GPIO;

  // Attach the LCD to the SPI bus:
  spi_device_interface_config_t devcfg =
  {
    .clock_speed_hz = 20000000, // Nominally 20000000. Can it be higher? Set to 10000000 for debugging with 24MHz logic analyzer.
    .mode = 0, // SPI mode 0.
    .spics_io_num = i_CSX_GPIO,
    .queue_size = SPI_MaxNumTransactions,
    .pre_cb = ILI9341_SPI_PreTransferCallback,  // Specify pre-transfer callback to set chip D/C pin.
    .flags = SPI_DEVICE_HALFDUPLEX // JSB: Added. Required for high speed operation (above 26 MHz)?
  };
  ret = spi_bus_add_device(HostDevice, &devcfg, &spi);
  assert(ret==ESP_OK);

  int CommandIndex = 0;
  const ILI9341_InitializationCommand_t *pCommand;

  // Initialize the non-SPI GPIOs:
  gpio_set_direction(i_D_CX_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_direction(i_ResetX_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_direction(i_BacklightX_GPIO, GPIO_MODE_OUTPUT);

  // Reset the display:
  gpio_set_level(i_ResetX_GPIO, 0);
  vTaskDelay(100 / portTICK_PERIOD_MS); // !!!JSB: Longer than necessary?
  gpio_set_level(i_ResetX_GPIO, 1);
  vTaskDelay(100 / portTICK_PERIOD_MS); // !!!JSB: Longer than necessary?

  // Send the initialization commands:
  while (1)
  {
    pCommand = &ILI9341_InitializationCommands[CommandIndex];

    if (pCommand->NumDataBytes == 0xff)
      break;

    ILI9341_SendCommand(spi, pCommand->Command);
    ILI9341_SendData(spi, pCommand->Data, pCommand->NumDataBytes & 0x1F);

    if (pCommand->NumDataBytes & 0x80)
      vTaskDelay(100 / portTICK_PERIOD_MS);

    ++CommandIndex;
  }

  // Enable backlight:
  gpio_set_level(i_BacklightX_GPIO, 1); // JSB: Displays vary as to whether they require 0 or 1 here.
}

//void ILI9341_Initialize()
/////////////////////////////////////////////////////////////////////////////////
//// Minimum configuration:
//// ILI9341_WriteCommand(ILI9341_MADCTL); // Memory Access Control
//// ILI9341_Write8(0x48);
////
//// ILI9341_WriteCommand(ILI9341_PIXFMT);
//// ILI9341_Write8(0x55); // DPI=[6:4] DBI=[2:0]
////
//// ILI9341_WriteCommand(ILI9341_SLPOUT); // Exit sleep
//// HAL_Delay(120);
//// ILI9341_WriteCommand(ILI9341_DISPON); // Display on
/////////////////////////////////////////////////////////////////////////////////
//{
//  ILI9341_SetDefaults();
//
//  HAL_GPIO_WritePin(ILI9341_RESX_GPIO_Port, ILI9341_RESX_GPIO, GPIO_GPIO_RESET);
//  HAL_Delay(1);
//  HAL_GPIO_WritePin(ILI9341_RESX_GPIO_Port, ILI9341_RESX_GPIO, GPIO_GPIO_SET);
//  HAL_Delay(5);
//
//  ILI9341_CSX_Low();
//
//  ILI9341_WriteCommand(0xEF);
//  ILI9341_Write8(0x03);
//  ILI9341_Write8(0x80);
//  ILI9341_Write8(0x02);
//
//  ILI9341_WriteCommand(LCD_POWERB);
//  ILI9341_Write8(0x00);
//  ILI9341_Write8(0XC1);
//  ILI9341_Write8(0X30);
//
//  ILI9341_WriteCommand(LCD_POWER_SEQ);
//  ILI9341_Write8(0x64);
//  ILI9341_Write8(0x03);
//  ILI9341_Write8(0X12);
//  ILI9341_Write8(0X81);
//
//  ILI9341_WriteCommand(LCD_DTCA);
//  ILI9341_Write8(0x85);
//  ILI9341_Write8(0x00);
//  ILI9341_Write8(0x78);
//
//  ILI9341_WriteCommand(LCD_POWERA);
//  ILI9341_Write8(0x39);
//  ILI9341_Write8(0x2C);
//  ILI9341_Write8(0x00);
//  ILI9341_Write8(0x34);
//  ILI9341_Write8(0x02);
//
//  ILI9341_WriteCommand(LCD_PRC);
//  ILI9341_Write8(0x20);
//
//  ILI9341_WriteCommand(LCD_DTCB);
//  ILI9341_Write8(0x00);
//  ILI9341_Write8(0x00);
//
//  ILI9341_WriteCommand(ILI9341_PWCTR1); // Power control
//  ILI9341_Write8(0x23); // VRH=[5:0]
//
//  ILI9341_WriteCommand(ILI9341_PWCTR2); // Power control
//  ILI9341_Write8(0x10); // BT=[3:0]
//
//  ILI9341_WriteCommand(ILI9341_VMCTR1); // VCOM control
//  ILI9341_Write8(0x3e); // VMH=[6:0]
//  ILI9341_Write8(0x28); // VML=[6:0]
//
//  ILI9341_WriteCommand(ILI9341_VMCTR2); // VCOM control 2
//  ILI9341_Write8(0x86); // VMF=[6:0]
//
//  ILI9341_WriteCommand(ILI9341_MADCTL); // Memory Access Control
//  ILI9341_Write8(0x48);
//
//  ILI9341_WriteCommand(ILI9341_VSCRSADD); // Vertical scroll
//  ILI9341_Write16(0); // Zero
//
//  ILI9341_WriteCommand(ILI9341_PIXFMT);
//  ILI9341_Write8(0x55); // DPI=[6:4] DBI=[2:0]
//
//  ILI9341_WriteCommand(ILI9341_FRMCTR1);
//  ILI9341_Write8(0x00);
//  ILI9341_Write8(0x18);
//
//  ILI9341_WriteCommand(ILI9341_DFUNCTR); // Display Function Control
//  ILI9341_Write8(0x08);
//  ILI9341_Write8(0x82);
//  ILI9341_Write8(0x27);
//
//
////  ILI9341_WriteCommand(ILI9341_WCD); // JSB
////  ILI9341_Write8(0xFF);
//
//
//  ILI9341_WriteCommand(0xF2); // 3Gamma
//  ILI9341_Write8(0x00); // Disable
//
//  ILI9341_WriteCommand(ILI9341_GAMMASET); // Gamma curve selected
//  ILI9341_Write8(0x01);
//  //
//  ILI9341_WriteCommand(ILI9341_GMCTRP1); // Set Gamma
//  ILI9341_Write8(0x0F);
//  ILI9341_Write8(0x31);
//  ILI9341_Write8(0x2B);
//  ILI9341_Write8(0x0C);
//  ILI9341_Write8(0x0E);
//  ILI9341_Write8(0x08);
//  ILI9341_Write8(0x4E);
//  ILI9341_Write8(0xF1);
//  ILI9341_Write8(0x37);
//  ILI9341_Write8(0x07);
//  ILI9341_Write8(0x10);
//  ILI9341_Write8(0x03);
//  ILI9341_Write8(0x0E);
//  ILI9341_Write8(0x09);
//  ILI9341_Write8(0x00);
//  //
//  ILI9341_WriteCommand(ILI9341_GMCTRN1); // Set Gamma
//  ILI9341_Write8(0x00);
//  ILI9341_Write8(0x0E);
//  ILI9341_Write8(0x14);
//  ILI9341_Write8(0x03);
//  ILI9341_Write8(0x11);
//  ILI9341_Write8(0x07);
//  ILI9341_Write8(0x31);
//  ILI9341_Write8(0xC1);
//  ILI9341_Write8(0x48);
//  ILI9341_Write8(0x08);
//  ILI9341_Write8(0x0F);
//  ILI9341_Write8(0x0C);
//  ILI9341_Write8(0x31);
//  ILI9341_Write8(0x36);
//  ILI9341_Write8(0x0F);
//
//  ILI9341_WriteCommand(ILI9341_SLPOUT); // Exit sleep
//  HAL_Delay(120);
//  ILI9341_WriteCommand(ILI9341_DISPON); // Display on
//
//  ILI9341_CSX_High();
//}

static spi_transaction_t SPI_Transactions[SPI_MaxNumTransactions]; // These must persist for the duration of the transaction. Store them so that the DMA can access them.
static int16_t SPI_NumTransactions = 0;

void SPI_Transactions_AddToQueue(spi_transaction_t *i_pTransaction)
{
  spi_transaction_t *pTransaction;
  esp_err_t ret;

  assert(SPI_NumTransactions < SPI_MaxNumTransactions);

  pTransaction = &SPI_Transactions[SPI_NumTransactions];

  *pTransaction = *i_pTransaction;

  ret = spi_device_queue_trans(spi, pTransaction, portMAX_DELAY);
  assert(ret==ESP_OK);

  ++SPI_NumTransactions;
}

void SPI_Transactions_WaitForCompletion()
{
  int16_t TransactionIndex;
  spi_transaction_t *pTransaction;
  esp_err_t ret;

  for (TransactionIndex = 0; TransactionIndex < SPI_NumTransactions; ++TransactionIndex)
  {
    ret = spi_device_get_trans_result(spi, &pTransaction, portMAX_DELAY);
    assert(ret == ESP_OK);
  }

  SPI_NumTransactions = 0;
}

static void ILI9341_SetColumnAddresses(int16_t X, int16_t Width)
{
  int16_t X1, X2;
  spi_transaction_t Transaction;

  X1 = X;
  X2 = X + Width - 1;

  memset(&Transaction, 0, sizeof(spi_transaction_t));
  Transaction.tx_data[0] = ILI9341_CASET; // Column address set
  Transaction.flags = SPI_TRANS_USE_TXDATA;
  Transaction.length = 8; // Data length (in bits)
  Transaction.user = (void *) 0; // Command.
  SPI_Transactions_AddToQueue(&Transaction);
  //
  memset(&Transaction, 0, sizeof(spi_transaction_t));
  Transaction.tx_data[0] = X1 >> 8; // Start column (High byte)
  Transaction.tx_data[1] = X1 & 0xFF; // Start column (Low byte)
  Transaction.tx_data[2] = X2 >> 8; // End column (High byte)
  Transaction.tx_data[3] = X2 & 0xFF; // End column (Low byte)
  Transaction.flags = SPI_TRANS_USE_TXDATA;
  Transaction.length = 4 * 8; // Data length (in bits)
  Transaction.user = (void *) 1; // Data
  SPI_Transactions_AddToQueue(&Transaction);
}

static void ILI9341_SetPageAddresses(int16_t Y, int16_t Height)
{
  int16_t Y1, Y2;
  spi_transaction_t Transaction;

  Y1 = Y;
  Y2 = Y + Height - 1;

  memset(&Transaction, 0, sizeof(spi_transaction_t));
  Transaction.tx_data[0] = ILI9341_PASET; // Page address set
  Transaction.flags = SPI_TRANS_USE_TXDATA;
  Transaction.length = 8; // Data length (in bits)
  Transaction.user = (void *) 0; // Command
  SPI_Transactions_AddToQueue(&Transaction);
  //
  memset(&Transaction, 0, sizeof(spi_transaction_t));
  Transaction.tx_data[0] = Y1 >> 8; // Start page (High byte)
  Transaction.tx_data[1] = Y1 & 0xFF; // Start page (Low byte)
  Transaction.tx_data[2] = Y2 >> 8; // End page (High byte)
  Transaction.tx_data[3] = Y2 & 0xFF; // End page (Low byte)
  Transaction.flags = SPI_TRANS_USE_TXDATA;
  Transaction.length = 4 * 8; // Data length (in bits)
  Transaction.user = (void *) 1; // Data
  SPI_Transactions_AddToQueue(&Transaction);
}

static void ILI9341_RAMWrite_ComandOnly()
{
  spi_transaction_t Transaction;

  memset(&Transaction, 0, sizeof(spi_transaction_t));
  Transaction.tx_data[0] = ILI9341_RAMWR;
  Transaction.flags = SPI_TRANS_USE_TXDATA;
  Transaction.length = 8; // Data length (in bits)
  Transaction.user = (void *) 0; // Command
  SPI_Transactions_AddToQueue(&Transaction);
}

static void ILI9341_RAMWrite_DataOnly(uint16_t *pPixels, int16_t NumPixels)
{
  spi_transaction_t Transaction;

  memset(&Transaction, 0, sizeof(spi_transaction_t));
  Transaction.tx_buffer = pPixels;
  Transaction.flags = 0;
  Transaction.length = 2 * NumPixels * 8;
  Transaction.user = (void *) 1; // Data
  SPI_Transactions_AddToQueue(&Transaction);
}

static void ILI9341_RAMWrite(uint16_t *pPixels, int16_t NumPixels)
{
  ILI9341_RAMWrite_ComandOnly();
  ILI9341_RAMWrite_DataOnly(pPixels, NumPixels);
}

void ILI9341_DrawPixels_MSBFirst(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height, uint16_t *pPixels)
// Supplied pixel data must be byte swapped.
{
  if ((Width == 0) || (Height == 0))
    return;

  ILI9341_SetColumnAddresses(X, Width);
  ILI9341_SetPageAddresses(Y, Height);
  ILI9341_RAMWrite(pPixels, Width * Height);
  SPI_Transactions_WaitForCompletion(); // !!! Keep this?
}

void ILI9341_DrawPixel(int16_t X, int16_t Y, uint16_t Color)
{
  uint16_t Color_MSBFirst;

  Color_MSBFirst = ILI9341_SwapBytes(Color);
  ILI9341_DrawPixels_MSBFirst(X, Y, 1, 1, &Color_MSBFirst);
  SPI_Transactions_WaitForCompletion();
}

#define PixelBuffer_MaxNumPixels 512
static uint16_t PixelBuffer[PixelBuffer_MaxNumPixels];

void ILI9341_DrawBar(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height, uint16_t Color)
{
  uint32_t RemainingNumPixelsToSend, NumPixelsToSetupInPixelBuffer, NumPixelsToSend;
  uint16_t Color_MSBFirst;

  if ((Width == 0) || (Height == 0))
    return;

  ESP_LOGV(LOG_TAG, "DrawBar: Begin");
	{
		RemainingNumPixelsToSend = Width * Height;
		NumPixelsToSetupInPixelBuffer = min32(RemainingNumPixelsToSend, PixelBuffer_MaxNumPixels);

		// Setup buffer:
		Color_MSBFirst = ILI9341_SwapBytes(Color);
		for (int16_t PixelIndex = 0; PixelIndex < NumPixelsToSetupInPixelBuffer; ++PixelIndex)
			PixelBuffer[PixelIndex] = Color_MSBFirst;

		ESP_ERROR_CHECK(spi_device_acquire_bus(spi, portMAX_DELAY));
		{
		// Setup chip addresses:
		ILI9341_SetColumnAddresses(X, Width);
		ILI9341_SetPageAddresses(Y, Height);

		// Send pixels:
		ILI9341_RAMWrite_ComandOnly();
		while (RemainingNumPixelsToSend > 0)
		{
				NumPixelsToSend = min32(RemainingNumPixelsToSend, PixelBuffer_MaxNumPixels);
			ILI9341_RAMWrite_DataOnly(PixelBuffer, NumPixelsToSend);
			SPI_Transactions_WaitForCompletion(); // Wait here so that PixelBuffer is not overwritten before the DMA has finished with it.
			RemainingNumPixelsToSend -= NumPixelsToSend;
		}
		}
		spi_device_release_bus(spi);
	}
  ESP_LOGV(LOG_TAG, "DrawBar: End");
}

void ILI9341_Clear(uint16_t Color)
{
  ILI9341_DrawBar(0, 0, ILI9341_Width, ILI9341_Height, Color);
}

const GFXfont *ILI9341_SetFont(const GFXfont *i_pFont)
{
  const GFXfont *Result;

  Result = pFont;
  pFont = i_pFont;
  return Result;
}

uint8_t ILI9341_GetFontYSpacing()
{
  return pFont->yAdvance;
}

static uint8_t IsNonPrintingChar(uint8_t Ch)
{
  return ((Ch < pFont->first) || (Ch > pFont->last));
}

uint16_t GetCharWidth(uint8_t Ch)
{
  if (IsNonPrintingChar(Ch))
    return 0;

  Ch -= pFont->first;
  GFXglyph *pGlyph = &pFont->pGlyph[Ch];
  uint8_t w = pGlyph->width;
  int8_t xo = pGlyph->xOffset;

  if (w == 0)
    return pGlyph->xAdvance;

  return xo + w;
}

uint16_t ILI9341_GetTextWidth(const char *Text)
{
  uint16_t TotalWidth, NumChars;
  const char *pText;

  NumChars = strlen(Text);

  pText = Text;

  TotalWidth=0;

  for (uint16_t CharIndex = 0; CharIndex < NumChars; ++CharIndex)
    TotalWidth += GetCharWidth(*pText++);

  return TotalWidth;
}

uint8_t ILI9341_DrawCharAtXY(uint8_t Ch, uint16_t X, uint16_t Y, uint16_t Color)
// X: X position of left edge of char.
// Y: Y position of line on which the char sits. The char may go below this line (e.g. g j p q y).
// Returns required X advance.
// Based on Adafruit_GFX.cpp.
{
  if (!pFont)
    return 0;
  if (IsNonPrintingChar(Ch))
    return 0;

  Ch -= pFont->first;
  GFXglyph *pGlyph = &pFont->pGlyph[Ch];
  uint8_t *pBitmap = pFont->pBitmap;

  uint16_t bo = pGlyph->bitmapOffset;
  uint8_t w = pGlyph->width, h = pGlyph->height;
  int8_t xo = pGlyph->xOffset, yo = pGlyph->yOffset;
  uint8_t xx, yy, bits = 0, bit = 0;
  int8_t yo_min = pFont->yOffsetMin, yo_max = pFont->yOffsetMax;

  uint16_t Color_MSBFirst, TextBackgroundColor_MSBFirst;
  uint16_t *pMemChar, *pMemCharPixel;
  uint8_t CharWidth, CharHeight;

  switch(TextDrawMode)
  {
    case tdmNone:
      break;

    case tdmThisCharBar:
      Color_MSBFirst = ILI9341_SwapBytes(Color);
      TextBackgroundColor_MSBFirst = ILI9341_SwapBytes(TextBackgroundColor);
      pMemChar = (uint16_t *)malloc(w * h * 2);
      pMemCharPixel = pMemChar;

      for (yy = 0; yy < h; ++yy)
      {
        for (xx = 0; xx < w; ++xx)
        {
          if (!(bit++ & 7))
            bits = pBitmap[bo++];
          *pMemCharPixel = bits & 0x80 ? Color_MSBFirst : TextBackgroundColor_MSBFirst;
          ++pMemCharPixel;
          bits <<= 1;
        }
      }
      ILI9341_DrawPixels_MSBFirst(X + xo, Y + yo, w, h, pMemChar);
      SPI_Transactions_WaitForCompletion();
      free(pMemChar);
      break;

    case tdmAnyCharBar:
      Color_MSBFirst = ILI9341_SwapBytes(Color);
      TextBackgroundColor_MSBFirst = ILI9341_SwapBytes(TextBackgroundColor);
      CharWidth = pGlyph->xAdvance;
      CharHeight = yo_max - yo_min + 1;
      pMemChar = (uint16_t *)malloc(CharWidth * CharHeight * sizeof(uint16_t));

      // Slow?
      pMemCharPixel = pMemChar;
      for (uint16_t PixelIndex = 0; PixelIndex < CharWidth * CharHeight; ++PixelIndex)
        *pMemCharPixel++ = TextBackgroundColor_MSBFirst;

      for (yy = 0; yy < h; ++yy)
      {
        pMemCharPixel = &pMemChar[(- yo_min + yo + yy) * CharWidth + xo];

        for (xx = 0; xx < w; ++xx)
        {
          if (!(bit++ & 7))
            bits = pBitmap[bo++];
          if (bits & 0x80)
            *pMemCharPixel = Color_MSBFirst;
          ++pMemCharPixel;
          bits <<= 1;
        }
      }
      ILI9341_DrawPixels_MSBFirst(X, Y + yo_min, CharWidth, CharHeight, pMemChar);
      SPI_Transactions_WaitForCompletion();
      free(pMemChar);
      break;

    case tdmMergeWithExistingPixels:
      for (yy = 0; yy < h; ++yy)
      {
        for (xx = 0; xx < w; ++xx)
        {
          if (!(bit++ & 7))
            bits = pBitmap[bo++];
          if (bits & 0x80)
            ILI9341_DrawPixel(X + xo + xx, Y + yo + yy, Color);
          bits <<= 1;
        }
      }
      break;
  }

  if (w == 0)
    return pGlyph->xAdvance;
  return xo + w;
}

uint16_t ILI9341_SetTextColor(uint16_t Value)
{
  uint16_t Result;

  Result = TextColor;
  TextColor = Value;
  return Result;
}

uint16_t ILI9341_SetTextBackgroundColor(uint16_t Value)
{
  uint16_t Result;

  Result = TextBackgroundColor;
  TextBackgroundColor = Value;
  return Result;
}

TextDrawMode_t ILI9341_SetTextDrawMode(TextDrawMode_t Value)
{
  TextDrawMode_t Result;

  Result = TextDrawMode;
  TextDrawMode = Value;
  return Result;
}

void ILI9341_DrawTextAtXY(const char *Text, uint16_t X, uint16_t Y, TextPosition_t TextPosition)
{
  uint8_t *pText;
  uint8_t Ch;
  uint16_t NumChars;
  uint8_t DX;

  pText = (uint8_t *) Text;

  if (!pText)
    return;

  NumChars = strlen(Text);

  switch (TextPosition)
  {
    case tpCentre:
      X -= ILI9341_GetTextWidth(Text) / 2;
      break;
    case tpRight:
      X -= ILI9341_GetTextWidth(Text);
      break;
    default:
      break;
  }

  for (uint16_t CharIndex = 0; CharIndex < NumChars; ++CharIndex)
  {
    Ch = *pText;
    DX = ILI9341_DrawCharAtXY(Ch, X, Y, TextColor);
    ++pText;
    X += DX;
  }
}

void ILI9341_Test_DrawGrid()
{
  int Index, X, Y;

  for (Index = 0; Index < 10; ++Index)
  {
    Y = 35 * Index;
    ILI9341_DrawBar(0, Y, 234, 1, ILI9341_COLOR_WHITE);
  }

  for (Index = 0; Index < 10; ++Index)
  {
    X = 26 * Index;
    ILI9341_DrawBar(X, 0, 1, 315, ILI9341_COLOR_WHITE);
  }
}

