# **AnimatedGIFS_SD**
Display GIFs on an LED Matrix and OLED Screen

This project uses exclusively the ESP32 and requires the PxMatrix library available at 2dom/PxMatrix

All wiring included as comments within the project

Gifs are processed by reading the contents of a file on an SD Card into a vector, which is then read from. The \#1 incompatibility with the original AnimatedGIFS_SD and the PxMatrix library is that PxMatrix will crash if any other form of SPI is used while the screen is displaying, so by storing the entire file as a vector on the ESP32 itself (which has more than enough ram for many realistic applications) this crash can be bypassed

Set DEBUG to 0, 2, 3 in GifDecoderImpl.h  

Many thanks to those included in the Readme as well as the original repository this is forked from
