/**
 * @file       pixmaps.cpp
 * @author     Dave Gutz
 * @license    GPL
 * @copyright  Copyright (c) 2015 Dave Gutz
 * @date       Nov 2015
 * @brief      Pixel maps for drawing on 8x8 LED
 *
 */
#include "pixmaps.h"
const uint8_t dots[] = {
    0b11000000,
    0b00110000,
    0b00001100,
    0b00000011,
};
const uint8_t plus1[] = {
    0b00100000,
    0b00010000,
    0b00001000,
    0b00000100,
};
const uint8_t plus5[] = {
    0b11111000,
    0b01111100,
    0b00111110,
    0b00011111,
};
const uint8_t dotmatzero[]= {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000
};

// Generate random dot on 8x8 LED pixel map
uint8_t* randomDot(void)
{
    static uint8_t dotmat[8];
    for (int i=0; i<8; i++) dotmat[i] = dotmatzero[i];
<<<<<<< HEAD
    int irand = random(4)*2;
=======
    int irand = random(4);
>>>>>>> origin/master
    int jrand = random(4);
    for (int i=irand; i<irand+2; i++) dotmat[i] = dots[jrand];
    return dotmat;
}


// Generate random plus sign on 8x8 LED pixel map
uint8_t* randomPlus(void)
{
    static uint8_t plusmat[8];
    for (int i=0; i<8; i++) plusmat[i] = dotmatzero[i];
    int irand = random(4);
    int jrand = random(4);
    plusmat[irand]      = plus1[jrand];
    plusmat[irand+1]    = plus1[jrand];
    plusmat[irand+2]    = plus5[jrand];
    plusmat[irand+3]    = plus1[jrand];
    plusmat[irand+4]    = plus1[jrand];
    return plusmat;
}
