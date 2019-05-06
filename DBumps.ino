  /**************************************
  Uses the following excellent libraries
      https://github.com/nhatuan84/esp32-sh1106-oled
      https://github.com/2dom/PxMatrix

  Uses the following pieces of hardware
    *ESP32
    *64x32 LED Matrix
    *128x64 OLED Arduino Sheild

  Thanks to these videos and creators for their massive help
    *https://www.youtube.com/watch?v=w3VIxtLPuRE (SD Card SPI)

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

PxMATRIX MATRIX(matrix_width,matrix_height,P_LAT, P_OE,P_A,P_B,P_C,P_D);//Initializes the Matrix Code and Pins

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
volatile int minicount = 0;
boolean DemoMode = true;
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 30;    // the debounce time; increase if the output flickers
unsigned long totalDebounce = 0;

  
//MUST BE 10-50 FOR SOME REASON FOR THE MATRIX
uint8_t display_draw_time=10;

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

// ISR for matrix display refresh
void IRAM_ATTR display_updater(){
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
  canvas[x][y]=pixel;
  MATRIX.drawPixel(x,y,pixel);
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
    for(int x = 0;x < matrix_width; x++)
  {
    for(int y = 0;y < matrix_height; y++)
    {
      drawNatPix(x,y,pixel);
    }
  }
}

void clearArrNat(uint16_t pixel)//Modifies array directly
{
  for(int x = 0;x < matrix_width; x++)
  {
    for(int y = 0;y < matrix_height; y++)
    {
      canvas[x][y]=pixel;
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
              buf565[n++]=canvas[x+i-1][y];
            }
            else
            {
            if(skip == -1 && pixel == transparentColorIndex) buf565[n++] = 0x0000;
            else buf565[n++] = palette[pixel];
            }
        }
        if (n) {
            /*MATRIX.setAddrWindow(x + i - n, y, endx, y);
            first = true;
            for (int j = 0; j < n; j++) (buf565[j]);*/ // Original Code to replicate
            for(int j=0;j<n;j++) drawNatPix(x+j,y,buf565[j]);
        }
    }
}

void displayUpdateTask( void * pvParameters )
{
while(69)
  {

    
int canvass[matrix_width][matrix_height];

    for(int x = 0;x < matrix_width; x++)
   {
    for(int y = 0;y < matrix_height; y++)
      {
        canvass[x][y]= /*Red*/ (0.299 * (((canvas[x][y] & 63488) >> 11) * 8)) + /*Green*/ (0.587 * (((canvas[x][y] & 2016) >> 5) * 4)) + /*Blue*/ (0.114 * (((canvas[x][y] & 31)) * 8));
      }
    }

    
for(int x = 0;x < matrix_width; x++)
   {
    for(int y = 0;y < matrix_height; y++)
      {
        if(canvass[x][y]<128)
         {
          
          if(x+1<matrix_width) canvass[x+1][y]=canvass[x+1][y]+((canvass[x][y]/16)*7);       //  7/16
          if(y+1<matrix_height)
          {
            if(x-1>=0) canvass[x-1][y+1]=canvass[x-1][y+1]+((canvass[x][y]/16)*3);            //  3/16
            if(1) canvass[x][y+1]=canvass[x][y+1]+((canvass[x][y]/16)*5);                     //  5/16
            if(x+1<matrix_width) canvass[x+1][y+1]=canvass[x+1][y+1]+((canvass[x][y]/16)*1);  //  1/16
          }
          canvass[x][y]=0;
         
            OLED.drawPixel(x*2+1,y*2+1, 0);
            OLED.drawPixel(x*2+1,y*2, 0);
            OLED.drawPixel(x*2,y*2+1, 0);
            OLED.drawPixel(x*2,y*2, 0);
          }
         else
          {
           
          if(x+1<matrix_width) canvass[x+1][y]=canvass[x+1][y]+(((canvass[x][y]-256)/16)*7);       //  7/16
          if(y+1<matrix_height)
          {
            if(x-1>=0) canvass[x-1][y+1]=canvass[x-1][y+1]+(((canvass[x][y]-256)/16)*3);            //  3/16
            if(1) canvass[x][y+1]=canvass[x][y+1]+(((canvass[x][y]-256)/16)*5);                     //  5/16
            if(x+1<matrix_width) canvass[x+1][y+1]=canvass[x+1][y+1]+(((canvass[x][y]-256)/16)*1);  //  1/16
          }
          canvass[x][y]=255;
          
           OLED.drawPixel(x*2+1,y*2+1, 1);
           OLED.drawPixel(x*2+1,y*2, 1);
           OLED.drawPixel(x*2,y*2+1, 1);
           OLED.drawPixel(x*2,y*2, 1);
         }
      
      }
    }

    
    OLED.display();
  }
}

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

/*
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
        index=1;
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

    while(1) 
    decoder.decodeFrame();
    */

    //////////////////////////////////////////////////////////////////////////
  ///////////// Setting up the Rotary Dial 
  //////////////////////////////////////////////////////////////////////////
  pinMode(R_Blue, INPUT_PULLUP); //FALSE = Contacted
  pinMode(R_White, INPUT_PULLUP); // FALSE = Contacted
  //////////////////////////////////////////////////////////////////////////

/*
  
display_update_enable(true);

  while(1)
  {
    if(!digitalRead(R_Blue))//When contacts touch
  {
    drawNatPix(0,1,0xFFFF);
    drawNatPix(0,0,0x0000);
  }
  else//When contacts are not touch
  {
    drawNatPix(0,0,0xFFFF);
    drawNatPix(0,1,0x0000);
  }
  if(!digitalRead(R_White))//When contacts touch
  {
    drawNatPix(1,1,0xFFFF);
    drawNatPix(1,0,0x0000);
  }
  else//When contacts are not touch
  {
    drawNatPix(1,0,0xFFFF);
    drawNatPix(1,1,0x0000);
  }
  if((digitalRead(R_Blue)!=PrevR_Blue)&&PrevR_Blue)
  {
    count++;
    Serial.print("Count:");
    Serial.println(count);
  }
  if((digitalRead(R_White)!=PrevR_White)&&!PrevR_White)
  {
    if(count!=0)
    {
      finalcount=count;
      Serial.print("Final Count:");
      Serial.println(finalcount);
      count=0;
      
    }
  }
  PrevR_Blue = digitalRead(R_Blue);
  PrevR_White = digitalRead(R_White);
  }

  */
}


void loop() {

    if(DemoMode==true)
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
    while(!digitalRead(R_White))
      {
         PrevR_Blue = !digitalRead(R_Blue);
         PrevR_White = true;
      }
      if((!digitalRead(R_White)!=PrevR_White)&&PrevR_White)
      {
        DemoMode=false;
        PrevR_Blue = !digitalRead(R_Blue);
        PrevR_White = !digitalRead(R_White);
      }
    decoder.decodeFrame();
    }
    
    else
    {
      while(!digitalRead(R_White))//While things are being entered into the dial
      {
        if(PrevR_White==false)//Before things are entered into the dial
        {
          totalDebounce=millis();
        }
      if((!digitalRead(R_Blue)!=PrevR_Blue)&&!PrevR_Blue&&((millis()-lastDebounceTime)>debounceDelay))
         {
             count++;
             minicount++;
             lastDebounceTime=millis();
         }
         PrevR_Blue = !digitalRead(R_Blue);
         PrevR_White = true;
      }
      if((!digitalRead(R_White)!=PrevR_White)&&PrevR_White&&(minicount!=0))//After things are entered into the dial
      {
             Serial.print("Count:");
             Serial.print(count);
             Serial.print("     miniCount:");
             Serial.print(minicount);
             totalDebounce=millis()-totalDebounce;
             Serial.print("     TotalTime:");
             Serial.println(totalDebounce);
        if(minicount>=10)
        {
          count=count-minicount;
        }
        if(count=0&&minicount!=0)   //If the first digit entered was 0, used for menu options
        {
          Serial.println("Options Mode");
          count=0;
        }
        else                        //If the first digit entered was 1-9, used for selecting gifs
        { 
          if((count/10)>=1)//Change 10 to 100 for opening the gif after 3 number inputs, 1000 for 4 number inputs, etc
          {
            Serial.print("Final Count:");
            Serial.println(count);
            if(count=99);
            {
              DemoMode=true;
            }
            count=0;
          }
          else
          {
            count=count*10;
            minicount=0;
          }
        }
        PrevR_White = false;
      }
      //decoder.decodeFrame();
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
