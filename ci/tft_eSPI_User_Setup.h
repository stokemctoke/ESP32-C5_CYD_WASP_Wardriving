// Minimal TFT_eSPI User_Setup.h for W.A.S.P. Nest (JC2432W328C / ILI9341).
// Copied over the installed TFT_eSPI library during CI; see README for pin details.
#define ILI9341_DRIVER
#define TFT_MISO  12
#define TFT_MOSI  13
#define TFT_SCLK  14
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST    4
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000
