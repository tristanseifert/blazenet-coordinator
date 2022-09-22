#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

// $[CMU]
// [CMU]$

// $[LFXO]
// [LFXO]$

// $[PRS.ASYNCH0]
// [PRS.ASYNCH0]$

// $[PRS.ASYNCH1]
// [PRS.ASYNCH1]$

// $[PRS.ASYNCH2]
// [PRS.ASYNCH2]$

// $[PRS.ASYNCH3]
// [PRS.ASYNCH3]$

// $[PRS.ASYNCH4]
// [PRS.ASYNCH4]$

// $[PRS.ASYNCH5]
// [PRS.ASYNCH5]$

// $[PRS.ASYNCH6]
// [PRS.ASYNCH6]$

// $[PRS.ASYNCH7]
// [PRS.ASYNCH7]$

// $[PRS.ASYNCH8]
// [PRS.ASYNCH8]$

// $[PRS.ASYNCH9]
// [PRS.ASYNCH9]$

// $[PRS.ASYNCH10]
// [PRS.ASYNCH10]$

// $[PRS.ASYNCH11]
// [PRS.ASYNCH11]$

// $[PRS.SYNCH0]
// [PRS.SYNCH0]$

// $[PRS.SYNCH1]
// [PRS.SYNCH1]$

// $[PRS.SYNCH2]
// [PRS.SYNCH2]$

// $[PRS.SYNCH3]
// [PRS.SYNCH3]$

// $[GPIO]
// GPIO SWCLK on PA01
//#define GPIO_SWCLK_PORT                          gpioPortA
//#define GPIO_SWCLK_PIN                           1

// GPIO SWDIO on PA02
//#define GPIO_SWDIO_PORT                          gpioPortA
//#define GPIO_SWDIO_PIN                           2

// GPIO SWV on PA03
//#define GPIO_SWV_PORT                            gpioPortA
//#define GPIO_SWV_PIN                             3

// [GPIO]$

// $[TIMER0]
// [TIMER0]$

// $[TIMER1]
// [TIMER1]$

// $[TIMER2]
// [TIMER2]$

// $[TIMER3]
// [TIMER3]$

// $[TIMER4]
// [TIMER4]$

// $[USART0]
// [USART0]$

// $[I2C1]
// [I2C1]$

// $[EUSART1]
// EUSART1 RX on PA08
#define EUSART1_RX_PORT                          gpioPortA
#define EUSART1_RX_PIN                           8

// EUSART1 TX on PA09
#define EUSART1_TX_PORT                          gpioPortA
#define EUSART1_TX_PIN                           9

// [EUSART1]$

// $[EUSART2]
// EUSART2 CS on PC00
#define EUSART2_CS_PORT                          gpioPortC
#define EUSART2_CS_PIN                           0

// EUSART2 RX on PC04
#define EUSART2_RX_PORT                          gpioPortC
#define EUSART2_RX_PIN                           4

// EUSART2 SCLK on PC03
#define EUSART2_SCLK_PORT                        gpioPortC
#define EUSART2_SCLK_PIN                         3

// EUSART2 TX on PC01
#define EUSART2_TX_PORT                          gpioPortC
#define EUSART2_TX_PIN                           1

// [EUSART2]$

// $[LCD]
// [LCD]$

// $[KEYSCAN]
// [KEYSCAN]$

// $[LETIMER0]
// [LETIMER0]$

// $[ACMP0]
// [ACMP0]$

// $[ACMP1]
// [ACMP1]$

// $[VDAC0]
// [VDAC0]$

// $[PCNT0]
// [PCNT0]$

// $[LESENSE]
// [LESENSE]$

// $[HFXO0]
// [HFXO0]$

// $[I2C0]
// [I2C0]$

// $[EUSART0]
// EUSART0 CS on PA07
#define EUSART0_CS_PORT                          gpioPortA
#define EUSART0_CS_PIN                           7

// EUSART0 RX on PA06
#define EUSART0_RX_PORT                          gpioPortA
#define EUSART0_RX_PIN                           6

// EUSART0 SCLK on PA04
#define EUSART0_SCLK_PORT                        gpioPortA
#define EUSART0_SCLK_PIN                         4

// EUSART0 TX on PA05
#define EUSART0_TX_PORT                          gpioPortA
#define EUSART0_TX_PIN                           5

// [EUSART0]$

// $[PTI]
// PTI DFRAME on PD01
#define PTI_DFRAME_PORT                          gpioPortD
#define PTI_DFRAME_PIN                           1

// PTI DOUT on PD00
#define PTI_DOUT_PORT                            gpioPortD
#define PTI_DOUT_PIN                             0

// [PTI]$

// $[MODEM]
// [MODEM]$

// $[CUSTOM_PIN_NAME]
#define HOST_nIRQ_PORT                           gpioPortA
#define HOST_nIRQ_PIN                            0

#define HOST_SPI_SCK_PORT                        gpioPortA
#define HOST_SPI_SCK_PIN                         4

#define HOST_SPI_MOSI_PORT                       gpioPortA
#define HOST_SPI_MOSI_PIN                        5

#define HOST_SPI_MISO_PORT                       gpioPortA
#define HOST_SPI_MISO_PIN                        6

#define HOST_SPI_nCS_PORT                        gpioPortA
#define HOST_SPI_nCS_PIN                         7

#define TTY_RX_PORT                              gpioPortA
#define TTY_RX_PIN                               8

#define TTY_TX_PORT                              gpioPortA
#define TTY_TX_PIN                               9

#define FLASH_SPI_SCK_PORT                       gpioPortC
#define FLASH_SPI_SCK_PIN                        0

#define FLASH_SPI_MISO_PORT                      gpioPortC
#define FLASH_SPI_MISO_PIN                       1

#define FLASH_SPI_MOSI_PORT                      gpioPortC
#define FLASH_SPI_MOSI_PIN                       4

#define LED_nATTN_PORT                           gpioPortD
#define LED_nATTN_PIN                            2

#define LED_nTX_PORT                             gpioPortD
#define LED_nTX_PIN                              3

#define LED_nRX_PORT                             gpioPortD
#define LED_nRX_PIN                              4

// [CUSTOM_PIN_NAME]$

#endif // PIN_CONFIG_H

