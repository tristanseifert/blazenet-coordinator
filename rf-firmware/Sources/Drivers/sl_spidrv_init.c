#include "spidrv.h"
#include "sl_spidrv_instances.h"
#include "sl_assert.h"


#include "sl_spidrv_eusart_flash_config.h"
#include "sl_spidrv_eusart_host_config.h"

SPIDRV_HandleData_t sl_spidrv_eusart_flash_handle_data;
SPIDRV_Handle_t sl_spidrv_eusart_flash_handle = &sl_spidrv_eusart_flash_handle_data;

SPIDRV_HandleData_t sl_spidrv_eusart_host_handle_data;
SPIDRV_Handle_t sl_spidrv_eusart_host_handle = &sl_spidrv_eusart_host_handle_data;

SPIDRV_Init_t sl_spidrv_eusart_init_flash = {
  .port = SL_SPIDRV_EUSART_FLASH_PERIPHERAL,
  .portTx = SL_SPIDRV_EUSART_FLASH_TX_PORT,
  .portRx = SL_SPIDRV_EUSART_FLASH_RX_PORT,
  .portClk = SL_SPIDRV_EUSART_FLASH_SCLK_PORT,
#if defined(SL_SPIDRV_EUSART_FLASH_CS_PORT)
  .portCs = SL_SPIDRV_EUSART_FLASH_CS_PORT,
#endif
  .pinTx = SL_SPIDRV_EUSART_FLASH_TX_PIN,
  .pinRx = SL_SPIDRV_EUSART_FLASH_RX_PIN,
  .pinClk = SL_SPIDRV_EUSART_FLASH_SCLK_PIN,
#if defined(SL_SPIDRV_EUSART_FLASH_CS_PIN)
  .pinCs = SL_SPIDRV_EUSART_FLASH_CS_PIN,
#endif
  .bitRate = SL_SPIDRV_EUSART_FLASH_BITRATE,
  .frameLength = SL_SPIDRV_EUSART_FLASH_FRAME_LENGTH,
  .dummyTxValue = 0,
  .type = SL_SPIDRV_EUSART_FLASH_TYPE,
  .bitOrder = SL_SPIDRV_EUSART_FLASH_BIT_ORDER,
  .clockMode = SL_SPIDRV_EUSART_FLASH_CLOCK_MODE,
  .csControl = SL_SPIDRV_EUSART_FLASH_CS_CONTROL,
  .slaveStartMode = SL_SPIDRV_EUSART_FLASH_SLAVE_START_MODE,
};

SPIDRV_Init_t sl_spidrv_eusart_init_host = {
  .port = SL_SPIDRV_EUSART_HOST_PERIPHERAL,
  .portTx = SL_SPIDRV_EUSART_HOST_TX_PORT,
  .portRx = SL_SPIDRV_EUSART_HOST_RX_PORT,
  .portClk = SL_SPIDRV_EUSART_HOST_SCLK_PORT,
#if defined(SL_SPIDRV_EUSART_HOST_CS_PORT)
  .portCs = SL_SPIDRV_EUSART_HOST_CS_PORT,
#endif
  .pinTx = SL_SPIDRV_EUSART_HOST_TX_PIN,
  .pinRx = SL_SPIDRV_EUSART_HOST_RX_PIN,
  .pinClk = SL_SPIDRV_EUSART_HOST_SCLK_PIN,
#if defined(SL_SPIDRV_EUSART_HOST_CS_PIN)
  .pinCs = SL_SPIDRV_EUSART_HOST_CS_PIN,
#endif
  .bitRate = SL_SPIDRV_EUSART_HOST_BITRATE,
  .frameLength = SL_SPIDRV_EUSART_HOST_FRAME_LENGTH,
  .dummyTxValue = 0,
  .type = SL_SPIDRV_EUSART_HOST_TYPE,
  .bitOrder = SL_SPIDRV_EUSART_HOST_BIT_ORDER,
  .clockMode = SL_SPIDRV_EUSART_HOST_CLOCK_MODE,
  .csControl = SL_SPIDRV_EUSART_HOST_CS_CONTROL,
  .slaveStartMode = SL_SPIDRV_EUSART_HOST_SLAVE_START_MODE,
};

void sl_spidrv_init_instances(void) {
#if !defined(SL_SPIDRV_USART_FLASH_CS_PIN)
  EFM_ASSERT(sl_spidrv_eusart_init_flash.csControl == spidrvCsControlAuto);
#endif 
#if !defined(SL_SPIDRV_USART_HOST_CS_PIN)
  EFM_ASSERT(sl_spidrv_eusart_init_host.csControl == spidrvCsControlAuto);
#endif 
  SPIDRV_Init(sl_spidrv_eusart_flash_handle, &sl_spidrv_eusart_init_flash);
  SPIDRV_Init(sl_spidrv_eusart_host_handle, &sl_spidrv_eusart_init_host);
}
