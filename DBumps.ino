/**************************************
  Uses the following excellent libraries
    https://github.com/nhatuan84/esp32-sh1106-oled
    https://github.com/2dom/PxMatrix
    https://github.com/dhepper/font8x8
    https://github.com/prenticedavid/AnimatedGIFs_SD
    
  Uses the following pieces of hardware
   ESP32
   64x32 LED Matrix
   128x64 OLED Arduino Sheild
   American Standard Rotary Dial
   MicroSD Breakout Board for Arduino

  Thanks to these videos and creators for their massive help
   https://www.youtube.com/watch?v=w3VIxtLPuRE (SD Card SPI)

  Dedicated to the brave Mujahadeen Fighters of Afghanistan
**************************************/

#define matrix_width 64
#define matrix_height 32

/*~~~~Used For The OLED~~~~~~~~~~~~~~*/
#include <Adafruit_SH1106.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SPITFT_Macros.h>
#include <gfxfont.h>
#include <Adafruit_SPITFT.h>
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/*~~~~Used For The Matrix~~~~~~~~~~~~*/
#include <Ticker.h> //Standard Libraries - Already Installed if you have ESP8266 set up
#include <PxMatrix.h>
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/*~~~~Used For The SD Card~~~~~~~~~~~*/
#include "FS.h"
#include "SD.h"
#include "SPI.h"
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

//#include <SD.h>
#ifndef min
#define min(a, b) (((a) <= (b)) ? (a) : (b))
#endif
#include "GifDecoder.h"
#include "FilenameFunctions.h"    //defines USE_SPIFFS

#define DISPLAY_TIME_SECONDS 80
#define GIFWIDTH 480 //228 fails on COW_PAINT

/*  template parameters are maxGifWidth, maxGifHeight, lzwMaxBits

    The lzwMaxBits value of 12 supports all GIFs, but uses 16kB RAM
    lzwMaxBits can be set to 10 or 11 for small displays, 12 for large displays
    All 32x32-pixel GIFs tested work with 11, most work with 10
*/

GifDecoder<GIFWIDTH, 320, 12> decoder;

#if defined(USE_SPIFFS)
#define GIF_DIRECTORY "/"     //ESP8266 SPIFFS
#define DISKCOLOUR   CYAN
#else
#define GIF_DIRECTORY "/gifs"
//#define GIF_DIRECTORY "/gifs32"
//#define GIF_DIRECTORY "/gifsdbg"
#define DISKCOLOUR   BLUE
#endif
#define LOOPFORSD

/********************************************\ FilenameFunctions
  |                                            |
  | //(PO is Pin Out of the Matrix)            |
  | //(IO is GPIO of the ESP32)                |
  | +---------+   Panel - Matrix Pins          |
  | |  R1 G1  |    R1   - IO13      G1   - POR2|
  | |  B1 GND |    B1   - POG2                 |
  | |  R2 G2  |    R2   - POR1      G2   - POG1|
  | |  B2 E   |    B2   - POB1      E    - GND |
  | |   A B   |    A    - IO26      B    - IO25|
  | |   C D   |    C    - IO33      D    - IO32|
  | | CLK LAT |    CLK  - IO14      LAT  - IO12|
  | | OEB GND |    OEB  - IO27                 |
  | +---------+                                |
  |                                            |
  \*******************************************/

//Defines Matrix Pins
#define P_LAT 12 //22 default
#define P_A 26//19 default
#define P_B 25 //23 default
#define P_C 33//18 default
#define P_D 32//5 default
#define P_OE 27 //2 default
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

PxMATRIX MATRIX(matrix_width, matrix_height, P_LAT, P_OE, P_A, P_B, P_C, P_D); //Initializes the Matrix Code and Pins

/***********************\
  |+-----------------+    |
  || VCC GND SCL SDA |    |ezgif-2-a6a14e0c6e29.gif
  |+-----------------+    |
  |Panel - OLED Pins      |
  |  VCC - 3.3V GND - GND |
  |  SCL - IO22 SDA - IO21|
  \***********************/

#define OLED_SDA 21
#define OLED_SCL 22

Adafruit_SH1106 OLED(OLED_SDA, OLED_SCL);//Initializes the OLED Display Code and Pins

/***************************\
  |+-------------------------+|
  || GND VCC MISO MOSI SCK CS||
  |+-------------------------+|
  |Panel  -  SD Card          |
  |  GND  - GND   VCC - 5V    |
  |  MISO - G19   SCK - G18   |
  |  MOSI - G23   CS  - G5    |
  \***************************/

//SPIClass spiSD(VSPI);
#define SD_CS 5
#define SDSPEED 40000000

/******************\
  |+----------------+|
  || BL+ BL- WH+ WH-||
  |+----------------+|
  |  Rotary Dial     |
  |  BL+ - G16       |
  |  BL- - GND       |
  |  WH+ - G04       |
  |  WH- - GND       |
  \*****************/
#define R_Blue 16
#define R_White 4
boolean PrevR_Blue = false;
boolean PrevR_White = false;
volatile int count = 0;
volatile int finalcount = 0;
boolean DemoMode = false;
boolean OptionMode = false;
//Blue Debouncer
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 80;    // the debounce time; increase if the output flickers
unsigned long totalDebounce = 0;

//White Debouncer
unsigned long lastDebounceTimeWhite = 0;  // the last time the output pin was toggled
unsigned long debounceDelayWhite = 120;    // the debounce time; increase if the output flickers
unsigned long totalDebounceWhite = 0;

//Clear the OLED in a lil bit
boolean clearSoon=false;
unsigned long clearMillis=0;
unsigned long clearDelay=5000;



//MUST BE 10-50 FOR SOME REASON FOR THE MATRIX
uint8_t display_draw_time = 10;

// Assign human-readable names to some common 16-bit color values:
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

int num_files;

#include "gifs_128.h"

uint16_t canvas[matrix_width][matrix_height];//Cache of currently displayed pixels
boolean OLEDMod[matrix_width][matrix_height];//2d array of pixels^2 the OLED is allowed to draw in, TRUE means it should not be drawn in, FALSE means it can be drawn in
int canvass[matrix_width][matrix_height];//What appears on the screen



        ////////////////////////////////////////////////////////////
        ////    8x8 FONT SECTION
        ////////////////////////////////////////////////////////////
/*
//Author: Daniel Hepper <daniel@hepper.net>
//License: Public Domain
//
//Encoding
//========
//Every character in the font is encoded row-wise in 8 bytes.
//
//The least significant bit of each byte corresponds to the first pixel in a
// row. 
//
//The character 'A' (0x41 / 65) is encoded as 
//{ 0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}
//
//
//    0x0C => 0000 1100 => ..XX....
//    0X1E => 0001 1110 => .XXXX...
//    0x33 => 0011 0011 => XX..XX..
//    0x33 => 0011 0011 => XX..XX..
//    0x3F => 0011 1111 => xxxxxx..
//    0x33 => 0011 0011 => XX..XX..
//    0x33 => 0011 0011 => XX..XX..
//    0x00 => 0000 0000 => ........
//
//To access the nth pixel in a row, right-shift by n.

//                         . . X X . . . .
//                         | | | | | | | |
//    (0x0C >> 0) & 1 == 0-+ | | | | | | |
//    (0x0C >> 1) & 1 == 0---+ | | | | | |
//    (0x0C >> 2) & 1 == 1-----+ | | | | |
//    (0x0C >> 3) & 1 == 1-------+ | | | |
//    (0x0C >> 4) & 1 == 0---------+ | | |
//    (0x0C >> 5) & 1 == 0-----------+ | |
//    (0x0C >> 6) & 1 == 0-------------+ |
//    (0x0C >> 7) & 1 == 0---------------+
 */
uint8_t HepperFont[128][8] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0000 (nul)
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0001
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0002
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0003
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0004
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0005
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0006
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0007
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0008
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0009
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000A
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000B
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000C
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000D
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000E
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+000F
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0010
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0011
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0012
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0013
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0014
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0015
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0016
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0017
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0018
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0019
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001A
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001B
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001C
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001D
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001E
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+001F
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0020 (space)
    { 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},   // U+0021 (!)
    { 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0022 (")
    { 0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00},   // U+0023 (#)
    { 0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00},   // U+0024 ($)
    { 0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00},   // U+0025 (%)
    { 0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00},   // U+0026 (&)
    { 0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0027 (')
    { 0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00},   // U+0028 (()
    { 0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00},   // U+0029 ())
    { 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},   // U+002A (*)
    { 0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00},   // U+002B (+)
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06},   // U+002C (,)
    { 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00},   // U+002D (-)
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00},   // U+002E (.)
    { 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00},   // U+002F (/)
    { 0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00},   // U+0030 (0)
    { 0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00},   // U+0031 (1)
    { 0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00},   // U+0032 (2)
    { 0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00},   // U+0033 (3)
    { 0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00},   // U+0034 (4)
    { 0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00},   // U+0035 (5)
    { 0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00},   // U+0036 (6)
    { 0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00},   // U+0037 (7)
    { 0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00},   // U+0038 (8)
    { 0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00},   // U+0039 (9)
    { 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00},   // U+003A (:)
    { 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06},   // U+003B (//)
    { 0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00},   // U+003C (<)
    { 0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00},   // U+003D (=)
    { 0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},   // U+003E (>)
    { 0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00},   // U+003F (?)
    { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},   // U+0040 (@)
    { 0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00},   // U+0041 (A)
    { 0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00},   // U+0042 (B)
    { 0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00},   // U+0043 (C)
    { 0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00},   // U+0044 (D)
    { 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00},   // U+0045 (E)
    { 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00},   // U+0046 (F)
    { 0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00},   // U+0047 (G)
    { 0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00},   // U+0048 (H)
    { 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},   // U+0049 (I)
    { 0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00},   // U+004A (J)
    { 0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00},   // U+004B (K)
    { 0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00},   // U+004C (L)
    { 0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00},   // U+004D (M)
    { 0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00},   // U+004E (N)
    { 0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00},   // U+004F (O)
    { 0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00},   // U+0050 (P)
    { 0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00},   // U+0051 (Q)
    { 0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00},   // U+0052 (R)
    { 0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00},   // U+0053 (S)
    { 0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},   // U+0054 (T)
    { 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00},   // U+0055 (U)
    { 0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},   // U+0056 (V)
    { 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},   // U+0057 (W)
    { 0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00},   // U+0058 (X)
    { 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00},   // U+0059 (Y)
    { 0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00},   // U+005A (Z)
    { 0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00},   // U+005B ([)
    { 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00},   // U+005C (\)
    { 0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00},   // U+005D (])
    { 0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00},   // U+005E (^)
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},   // U+005F (_)
    { 0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+0060 (`)
    { 0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00},   // U+0061 (a)
    { 0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00},   // U+0062 (b)
    { 0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00},   // U+0063 (c)
    { 0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00},   // U+0064 (d)
    { 0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00},   // U+0065 (e)
    { 0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00},   // U+0066 (f)
    { 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F},   // U+0067 (g)
    { 0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00},   // U+0068 (h)
    { 0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},   // U+0069 (i)
    { 0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E},   // U+006A (j)
    { 0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00},   // U+006B (k)
    { 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},   // U+006C (l)
    { 0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00},   // U+006D (m)
    { 0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00},   // U+006E (n)
    { 0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00},   // U+006F (o)
    { 0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F},   // U+0070 (p)
    { 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78},   // U+0071 (q)
    { 0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00},   // U+0072 (r)
    { 0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00},   // U+0073 (s)
    { 0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00},   // U+0074 (t)
    { 0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00},   // U+0075 (u)
    { 0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},   // U+0076 (v)
    { 0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00},   // U+0077 (w)
    { 0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00},   // U+0078 (x)
    { 0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F},   // U+0079 (y)
    { 0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00},   // U+007A (z)
    { 0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00},   // U+007B ({)
    { 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},   // U+007C (|)
    { 0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00},   // U+007D (})
    { 0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // U+007E (~)
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}    // U+007F
};


        ////////////////////////////////////////////////////////////
        ////    WRITING 8x8 TEXT TO THE OLED
        ////////////////////////////////////////////////////////////
        
//(Sets the X position of the top leftmost pixel of the character, Sets the Y position of the top leftmost pixel of the character, String to display,Length of the String, whether the message should be cleared after a period of time)
void canvassString(int xx, int yy, char *ascii,int asciilength,boolean shouldIClearSoon = false)//Only processes one line
{
  for(int i=0;i<asciilength;i++)
  {
    canvassChar(xx+(i*8),yy,&ascii[i]);
  }
  if(shouldIClearSoon)
  {
    clearMillis=millis();
    clearSoon=true;
  }
}

void canvassChar(int xx, int yy, char const *ascii)//Reliant on 8x8 fonts
{
  canvassBlockChunk(xx,yy,xx+8,yy+8);
  for(int y=yy;y<(yy+8) && y<matrix_height;y++)
  {
    for(int x=xx;x<(xx+8) && x<matrix_width;x++)
    {
      if((((HepperFont[int(ascii[0])][y-yy]) >> (x-xx)) & 1) == 1)
      {
        canvass[x][y]=255;
      }
      else
      {
        canvass[x][y]=0;
      }
    }
  }
}

void canvassBlockChunk(int xx, int yy, int xxx, int yyy)//Prevents this area on the OLED from being overwritten
{
  for(int x=xx;x<xxx && x<matrix_width;x++)
  {
    for(int y=yy;y<yyy && y<matrix_height;y++)
    {
      OLEDMod[x][y]=true;
    }
  }
}

void canvassUnblockChunk(int xx, int yy, int xxx, int yyy)//Allows this area on the OLED to be overwritten again
{
  for(int x=xx;x<xxx && x<matrix_width;x++)
  {
    for(int y=yy;y<yyy && y<matrix_height;y++)
    {
      OLEDMod[x][y]=false;
    }
  }
}

void canvassClear()//Allows the whole OLED to be written over again
{
  clearSoon=false;
  for(int x=0;x<matrix_width;x++)
  {
    for(int y=0;y<matrix_height;y++)
    {
      OLEDMod[x][y]=false;
    }
  }
}

        ////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////
        ////    RGB MATRIX FUNCTIONS
        ////////////////////////////////////////////////////////////
        
// ISR for matrix display refresh
void IRAM_ATTR display_updater() {
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  MATRIX.display(display_draw_time);
  portEXIT_CRITICAL_ISR(&timerMux);
}

void display_update_enable(bool is_enable)
{
  if (is_enable)
  {
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &display_updater, true);
    timerAlarmWrite(timer, 2000, true);
    timerAlarmEnable(timer);
  }
  else
  {
    timerDetachInterrupt(timer);
    timerAlarmDisable(timer);
  }
}

#define M0(x) {x, #x, sizeof(x)}
typedef struct {
  const unsigned char *data;
  const char *name;
  uint32_t sz;
} gif_detail_t;
gif_detail_t gifs[] = {

  M0(teakettle_128x128x10_gif),  // 21155
  //M0(globe_rotating_gif),        // 90533
  M0(bottom_128x128x17_gif),     // 51775
  //M0(irish_cows_green_beer_gif), // 29798
#if !defined(TEENSYDUINO)
  M0(horse_128x96x8_gif),        //  7868
#endif
#if defined(ESP32)
  //M0(llama_driver_gif),    //758945
#endif
  //    M0(marilyn_240x240_gif),       // 40843
  //    M0(cliff_100x100_gif),   //406564
};

        ////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////
        ////    GIF READING AND DISPLAYING FUNCTIONS
        ////////////////////////////////////////////////////////////
        
const uint8_t *g_gif;
uint32_t g_seek;
bool fileSeekCallback_P(unsigned long position) {
  g_seek = position;
  return true;
}

unsigned long filePositionCallback_P(void) {
  return g_seek;
}

int fileReadCallback_P(void) {
  return pgm_read_byte(g_gif + g_seek++);
}

int fileReadBlockCallback_P(void * buffer, int numberOfBytes) {
  memcpy_P(buffer, g_gif + g_seek, numberOfBytes);
  g_seek += numberOfBytes;
  return numberOfBytes; //.kbv
}

void screenClearCallback(void) {
  //    tft.fillRect(0, 0, 128, 128, 0x0000);
}

bool openGifFilenameByIndex_P(const char *dirname, int index)
{
  gif_detail_t *g = &gifs[index];
  g_gif = g->data;
  g_seek = 0;

  Serial.print("Flash: ");
  Serial.print(g->name);
  Serial.print(" size: ");
  Serial.println(g->sz);

  return index < num_files;
}
void updateScreenCallback(void) {

}

void drawNatPix(int16_t x, int16_t y, uint16_t pixel)//Special draw function that writes to the screen, an array storing all currently displayed pixels, and the OLED
{
  canvas[x][y] = pixel;
  MATRIX.drawPixel(x, y, pixel);
  /*
    if(pixel==0x0000)
    {
    OLED.drawPixel(x*2+1,y*2+1, 0);
    OLED.drawPixel(x*2+1,y*2, 0);
    OLED.drawPixel(x*2,y*2+1, 0);
    OLED.drawPixel(x*2,y*2, 0);
    }
    else
    {
    OLED.drawPixel(x*2+1,y*2+1, 2);
    OLED.drawPixel(x*2+1,y*2, 2);
    OLED.drawPixel(x*2,y*2+1, 2);
    OLED.drawPixel(x*2,y*2, 2);
    }
  */
}

void updateOLED()
{
  OLED.display();
}

void clearNat(uint16_t pixel)//Calls drawNatPix to clear the entire screen
{
  for (int x = 0; x < matrix_width; x++)
  {
    for (int y = 0; y < matrix_height; y++)
    {
      drawNatPix(x, y, pixel);
    }
  }
}

void clearArrNat(uint16_t pixel)//Modifies array directly
{
  for (int x = 0; x < matrix_width; x++)
  {
    for (int y = 0; y < matrix_height; y++)
    {
      canvas[x][y] = pixel;
    }
  }
}

void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue) {
  MATRIX.drawPixel(x, y, MATRIX.color565(red, green, blue));
}

void drawLineCallback(int16_t x, int16_t y, uint8_t *buf, int16_t w, uint16_t *palette, int16_t skip, int transparentColorIndex) {
  uint8_t pixel;
  bool first;
  if (y >= matrix_height || x >= matrix_width ) return;
  if (x + w > matrix_width) w = matrix_width - x;
  if (w <= 0) return;
  int16_t endx = x + w - 1;
  uint16_t buf565[w];
  for (int i = 0; i < w; ) {
    int n = 0;
    while (i < w) {
      pixel = buf[i++];
      if (pixel == skip)
      {
        buf565[n++] = canvas[x + i - 1][y];
      }
      else
      {
        if (skip == -1 && pixel == transparentColorIndex) buf565[n++] = 0x0000;
        else buf565[n++] = palette[pixel];
      }
    }
    if (n) {
      /*MATRIX.setAddrWindow(x + i - n, y, endx, y);
        first = true;
        for (int j = 0; j < n; j++) (buf565[j]);*/ // Original Code to replicate
      for (int j = 0; j < n; j++) drawNatPix(x + j, y, buf565[j]);
    }
  }
}

        ////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////
        ////    OLED DISPLAY
        ////////////////////////////////////////////////////////////
void displayUpdateTask( void * pvParameters )
{
  while (69)
  {
    
    for (int x = 0; x < matrix_width; x++)
    {
      for (int y = 0; y < matrix_height; y++)
      {
        if(!OLEDMod[x][y]) canvass[x][y] = /*Red*/ (0.299 * (((canvas[x][y] & 63488) >> 11) * 8)) + /*Green*/ (0.587 * (((canvas[x][y] & 2016) >> 5) * 4)) + /*Blue*/ (0.114 * (((canvas[x][y] & 31)) * 8));
      }
    }


    for (int x = 0; x < matrix_width; x++)
    {
      for (int y = 0; y < matrix_height; y++)
      {
        if (canvass[x][y] < 128)
        {

          if (!OLEDMod[x+1][y] && x + 1 < matrix_width) canvass[x + 1][y] = canvass[x + 1][y] + ((canvass[x][y] / 16) * 7); //  7/16
          if (y + 1 < matrix_height)
          {
            if (!OLEDMod[x-1][y+1] && x - 1 >= 0) canvass[x - 1][y + 1] = canvass[x - 1][y + 1] + ((canvass[x][y] / 16) * 3); //  3/16
            if (!OLEDMod[x+0][y+1]) canvass[x][y + 1] = canvass[x][y + 1] + ((canvass[x][y] / 16) * 5);        //  5/16
            if (!OLEDMod[x+1][y+1] && x + 1 < matrix_width) canvass[x + 1][y + 1] = canvass[x + 1][y + 1] + ((canvass[x][y] / 16) * 1); //  1/16
          }
          canvass[x][y] = 0;
          OLED.drawPixel(x * 2 + 1, y * 2 + 1, 0);
          OLED.drawPixel(x * 2 + 1, y * 2, 0);
          OLED.drawPixel(x * 2, y * 2 + 1, 0);
          OLED.drawPixel(x * 2, y * 2, 0);
        }
        else
        {

          if (!OLEDMod[x+1][y] && x + 1 < matrix_width) canvass[x + 1][y] = canvass[x + 1][y] + (((canvass[x][y] - 256) / 16) * 7); //  7/16
          if (y + 1 < matrix_height)
          {
            if (!OLEDMod[x-1][y+1] && x - 1 >= 0) canvass[x - 1][y + 1] = canvass[x - 1][y + 1] + (((canvass[x][y] - 256) / 16) * 3); //  3/16
            if (!OLEDMod[x+0][y+1]) canvass[x][y + 1] = canvass[x][y + 1] + (((canvass[x][y] - 256) / 16) * 5);      //  5/16
            if (!OLEDMod[x+1][y+1] && x + 1 < matrix_width) canvass[x + 1][y + 1] = canvass[x + 1][y + 1] + (((canvass[x][y] - 256) / 16) * 1); //  1/16
          }
          canvass[x][y] = 255;
          OLED.drawPixel(x * 2 + 1, y * 2 + 1, 1);
          OLED.drawPixel(x * 2 + 1, y * 2, 1);
          OLED.drawPixel(x * 2, y * 2 + 1, 1);
          OLED.drawPixel(x * 2, y * 2, 1);
        }

      }
    }


    OLED.display();
  }
}

void OLEDTextDisplay(int cursX, int cursY, char *string, int length)//Draws text that will stay on the OLED screen
{
  int limitX = cursX + (5 * length); //Assuming each character is 5 pixels wide
  int limitY = cursY + 8; //Assuming there is only one line and it's 8 pixels tall
  for (int x = cursX; x < limitX; x++)
  {
    for (int y = cursY; y < limitY; y++)
    {
      if (x < matrix_width)
      {
        if (y < matrix_height)
        {
          OLEDMod[x][y] = true;
          OLED.drawPixel(x * 2 + 1, y * 2 + 1, 0);
          OLED.drawPixel(x * 2 + 1, y * 2, 0);
          OLED.drawPixel(x * 2, y * 2 + 1, 0);
          OLED.drawPixel(x * 2, y * 2, 0);
        }
      }
    }
  }
  OLED.setCursor(cursX * 2, cursY * 2);
  OLED.setTextSize(2);
  OLED.setTextColor(2);
  OLED.println("1");
}

        ////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////
        ////    THE PROGRAM
        ////////////////////////////////////////////////////////////

        
// Setup method runs once, when the sketch starts
void setup() {
  Serial.begin(9600);//Connect Serial for Debugging
  OLED.begin(SH1106_SWITCHCAPVCC, 0x3C);//Initializes the OLED Display
  OLED.clearDisplay();//Wipes the OLED Display
  MATRIX.begin(16);//Initializes the MATRIX Display
  Serial.println("jef");
  clearArrNat(0x0000);//Sets the initial value of the canvas array to fully empty
  MATRIX.setBrightness(255);
  MATRIX.setFastUpdate(true);
  display_update_enable(true);
  display_update_enable(false);

  Serial.print("Core running on:");
  Serial.println(xPortGetCoreID());

  decoder.setScreenClearCallback(screenClearCallback);
  decoder.setUpdateScreenCallback(updateScreenCallback);
  decoder.setDrawPixelCallback(drawPixelCallback);
  decoder.setDrawLineCallback(drawLineCallback);

  decoder.setFileSeekCallback(fileSeekCallback);
  decoder.setFilePositionCallback(filePositionCallback);
  decoder.setFileReadCallback(fileReadCallback);
  decoder.setFileReadBlockCallback(fileReadBlockCallback);

  decoder.setUpdateOLED(updateOLED);


  xTaskCreatePinnedToCore(
    displayUpdateTask,   /* Function to implement the task */
    "coreTask", /* Name of the task */
    10000,      /* Stack size in words */
    NULL,       /* Task input parameter */
    0,          /* Priority of the task */
    NULL,       /* Task handle. */
    0);  /* Core where the task should run */

  Serial.println("AnimatedGIFs_SD");

canvassString(0,0,"Ins. SD",7);
#ifdef LOOPFORSD
  while (initSdCard(SD_CS) < 0) {
    Serial.println("No SD card");
  }
  num_files = enumerateGIFFiles(GIF_DIRECTORY, true);
#else
  if (initSdCard(SD_CS) < 0) {
    Serial.println("No SD card");
    decoder.setFileSeekCallback(fileSeekCallback_P);
    decoder.setFilePositionCallback(filePositionCallback_P);
    decoder.setFileReadCallback(fileReadCallback_P);
    decoder.setFileReadBlockCallback(fileReadBlockCallback_P);
    g_gif = gifs[0].data;
    for (num_files = 0; num_files < sizeof(gifs) / sizeof(*gifs); num_files++) {
      Serial.println(gifs[num_files].name);
    }
    while (1);
  }
  else {
    num_files = enumerateGIFFiles(GIF_DIRECTORY, true);
  }
#endif
canvassClear();

  //display_update_enable(true);
  // Determine how many animated GIF files exist
  Serial.print("Animated GIF files Found: ");
  Serial.println(num_files);

  if (num_files < 0) {
    Serial.println("No gifs directory");
    while (1);
  }

  if (!num_files) {
    Serial.println("Empty gifs directory");
    while (1);
  }
  
  //////////////////////////////////////////////////////////////////////////
  ///////////// Setting up the Rotary Dial
  //////////////////////////////////////////////////////////////////////////
  pinMode(R_Blue, INPUT_PULLUP); //FALSE = Contacted
  pinMode(R_White, INPUT_PULLUP); // FALSE = Contacted
  //////////////////////////////////////////////////////////////////////////

  attachInterrupt(R_Blue, blueInterrupt, FALLING);
  attachInterrupt(R_White, whiteInterrupt, CHANGE);

  canvassString(0,0,"Big",3);
  canvassString(0,8,"Dyke",4);
  canvassString(0,16,"Energy",6,true);
}


        ////////////////////////////////////////////////////////////
        ////    LOOP
        ////////////////////////////////////////////////////////////

void loop() {

  if (DemoMode == true)
  {
    static unsigned long futureTime, cycle_start;

    //    int index = random(num_files);
    static int index = -1;

    if (futureTime < millis() || decoder.getCycleNo() > 1) {
      display_update_enable(false);
      char buf[80];
      int32_t now = millis();
      int32_t frames = decoder.getFrameCount();
      int32_t cycle_design = decoder.getCycleTime();
      int32_t cycle_time = now - cycle_start;
      int32_t percent = (100 * cycle_design) / cycle_time;
      sprintf(buf, "[%ld frames = %ldms] actual: %ldms speed: %d%%",
              frames, cycle_design, cycle_time, percent);
      Serial.println(buf);
      cycle_start = now;
      //        num_files = 2;
      if (++index >= num_files) {
        index = 0;
      }
      clearArrNat(0x0000);
      OLED.clearDisplay();
      int good;
      if (g_gif) good = (openGifFilenameByIndex_P(GIF_DIRECTORY, index) >= 0);
      else
      {
        //display_update_enable(false);
        good = (openGifFilenameByIndex(GIF_DIRECTORY, index) >= 0);
        //display_update_enable(true);
      }
      if (good >= 0) {
        MATRIX.fillScreen(g_gif ? BLACK : BLACK);
        MATRIX.fillRect(GIFWIDTH, 0, 1, matrix_height, BLACK);
        MATRIX.fillRect(278, 0, 1, matrix_height, BLACK);

        decoder.startDecoding();

        // Calculate time in the future to terminate animation
        futureTime = millis() + (DISPLAY_TIME_SECONDS * 1000);
      }
      display_update_enable(true);
    }
    decoder.decodeFrame();
  }
  else
  {
    /*
      //decoder.decodeFrame();
    */
  }
  if(clearSoon)
  {
    if(millis()>(clearMillis+clearDelay))
    {
      canvassClear();
    }
  }
}

        ////////////////////////////////////////////////////////////
        ////    BLUE INTERRUPT
        ////////////////////////////////////////////////////////////
        
void blueInterrupt()
{
  if (millis() > (lastDebounceTime + debounceDelay))
  {
    Serial.print("Blue");
    lastDebounceTime = millis();
    count = count + 1;
    Serial.print("     ");
    Serial.println(count);

  }
}

        ////////////////////////////////////////////////////////////
        ////    WHITE INTERRUPT
        ////////////////////////////////////////////////////////////
        
void whiteInterrupt()
{

  if ((millis() > (lastDebounceTimeWhite + debounceDelayWhite)))
  {
    lastDebounceTimeWhite = millis();
    if (digitalRead(R_White) == false && (millis() > (lastDebounceTime + debounceDelay))) //If someone just started activating it, or FALLING
    {
      Serial.println("Activated White");
      count = 0;
    }
    else
    {
      if (digitalRead(R_White) == true) //If someone just ended inputting into it, or RISING
      {
        if(count>=10) count=0;
        ////////////////////////////////////////////////////////////
        ////    VERY IMPORTANT SECTION
        ////////////////////////////////////////////////////////////
        if(OptionMode)
        {
          Serial.print("OptionCount: ");
          Serial.println(count);
          char cister[2];
          cister[0]=':';
          cister[1]=count+'0';
          canvassString(40,0,cister,2,true);
          if(count==0)
          {
            
          }
          if(count==1)
          {
            
          }
          if(count==2)//Turns on DemoMode
          {
            DemoMode=!DemoMode;
          }
          if(count==3)
          {
            //////////////////////////////////////////////////////////////////
            static unsigned long futureTime, cycle_start;
            
              display_update_enable(false);
              
              char buf[80];
              int32_t now = millis();
              int32_t frames = decoder.getFrameCount();
              int32_t cycle_design = decoder.getCycleTime();
              int32_t cycle_time = now - cycle_start;
              int32_t percent = (100 * cycle_design) / cycle_time;
              sprintf(buf, "[%ld frames = %ldms] actual: %ldms speed: %d%%",
              frames, cycle_design, cycle_time, percent);
              Serial.println(buf);
              
              clearArrNat(0x0000);
              OLED.clearDisplay();
              int good;
              good = (openGifByFilename(GIF_DIRECTORY,"ezgif-2-2996c992b65d") >= 0);
              if (good >= 0) {
                MATRIX.fillScreen(g_gif ? BLACK : BLACK);
                MATRIX.fillRect(GIFWIDTH, 0, 1, matrix_height, BLACK);
                MATRIX.fillRect(278, 0, 1, matrix_height, BLACK);
                decoder.startDecoding();
              }
              display_update_enable(true);
            //////////////////////////////////////////////////////////////////
          }
          if(count==4)
          {
            
          }
          if(count==5)
          {
            
          }
          if(count==6)
          {
            MATRIX.setBrightness(30);
            canvassClear();
            canvassString(0,24,"B:30",4,true);
          }
          if(count==7)
          {
            MATRIX.setBrightness(90);
            canvassClear();
            canvassString(0,24,"B:90",4,true);
          }
          if(count==8)
          {
            MATRIX.setBrightness(160);
            canvassClear();
            canvassString(0,24,"B:160",5,true);
          }
          if(count==9)
          {
            MATRIX.setBrightness(255);
            canvassClear();
            canvassString(0,24,"B:255",5,true);
          }
          OptionMode=false;
        }
        else
        {
          finalcount=finalcount+count;
          if(finalcount==0)//If 0 is typed, activates option mode
          {
            OptionMode=true;
            Serial.println("OptionMode");
            canvassString(0,0,"Debug",5);
          }
          else
          {
            if(finalcount>=10)//If more than 1 digit has been entered
            {
              Serial.print("finalcount:");
              Serial.println(finalcount);
              char cister[2];
              itoa(finalcount,cister,10);
              canvassString(0,0,cister,2,true);
              finalcount=0;
            }
            else
            {
              char cister[1];
              cister[0]=finalcount+'0';
              canvassString(0,0,cister,1);
              finalcount=finalcount*10;
            }
          }
        }
        Serial.print("finalcount:");
        Serial.println(finalcount);
        Serial.println("Disabled White");
      }
    }
  }
}
/*
    Animated GIFs Display Code for SmartMatrix and 32x32 RGB LED Panels

    Uses SmartMatrix Library for Teensy 3.1 written by Louis Beaudoin at pixelmatix.com

    Written by: Craig A. Lindley

    Copyright (c) 2014 Craig A. Lindley
    Refactoring by Louis Beaudoin (Pixelmatix)

    Permission is hereby granted, free of charge, to any person obtaining a copy of
    this software and associated documentation files (the "Software"), to deal in
    the Software without restriction, including without limitation the rights to
    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
    the Software, and to permit persons to whom the Software is furnished to do so,
    subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
    FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
    COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
    IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
    This code first looks for .gif files in the /gifs/ directory
    (customize below with the GIF_DIRECTORY definition) then plays random GIFs in the directory,
    looping each GIF for DISPLAY_TIME_SECONDS or allowing you to select gifs 10.gif ... 99.gif

    This example is meant to give you an idea of how to add GIF playback to your own sketch.
    For a project that adds GIF playback with other features, take a look at
    Light Appliance and Aurora:
    https://github.com/CraigLindley/LightAppliance
    https://github.com/pixelmatix/aurora

    If you find any GIFs that won't play properly, please attach them to a new
    Issue post in the GitHub repo here:
    https://github.com/NatBF/DBumps
*/

/*
    CONFIGURATION:
    - If you're using SmartLED Shield V4 (or above), uncomment the line that includes <SmartMatrixShieldV4.h>
    - update the "SmartMatrix configuration and memory allocation" section to match the width and height and other configuration of your display
    - Note for 128x32 and 64x64 displays with Teensy 3.2 - need to reduce RAM:
      set kRefreshDepth=24 and kDmaBufferRows=2 or set USB Type: "None" in Arduino,
      decrease refreshRate in setup() to 90 or lower to get good an accurate GIF frame rate
    - Set the chip select pin for your board.  On Teensy 3.5/3.6, the onboard microSD CS pin is "BUILTIN_SDCARD"
*/
