# **D**igital**B**umper**Sticker**
Display GIFs on an LED Matrix and OLED Screen

This project uses exclusively the ESP32 and requires the PxMatrix library available at 2dom/PxMatrix

Gifs are processed by reading the contents of a file on an SD Card into a vector, which is then read from. The \#1 incompatibility with the original AnimatedGIFS_SD and the PxMatrix library is that PxMatrix will crash if any other form of SPI is used while the screen is displaying, so by storing the entire file as a vector on the ESP32 itself (which has more than enough ram for many realistic applications) this crash can be bypassed

Set DEBUG to 0, 2, 3 in GifDecoderImpl.h  

GIF optimisation via PC or online is not that clever.   
They should minimise the display rectangle to only cover animation changes.
They should make a compromisde between contiguous run of pixels or multiple transparent pixels.

Many thanks to Craig A. Lindley and Louis Beaudoin (Pixelmatix) for their original work on small LED matrix.
