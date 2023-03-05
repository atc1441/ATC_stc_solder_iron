#include "STC8xxxx_SDCC.h"
#include <stdio.h>
#include <stdint.h>
#include "font.h"

/*
Pinout of the STC Iron is:
P1.0 = ADC0 Analog in of Voltage main 12V-24V
P1.1 = ADC1 Analog in Temperature meassurement
P1.3 = ADC3 -----
P1.5 = ADC5 -----
P1.6 = ADC6 Connected to VCC 5V
P1.7 = ADC7 Connected to VCC 5V

P2.4 = i2c SDA = OLED
P2.5 = i2c SCL = OLED

P3.0 = Button Left Down --- Also RXD for Flashing
P3.1 = Button Push --- Also TXD for Flashing
P3.2 = Button Right Up
P3.3 = Shake Sensor
*/

void Wait()
{
    while (!(I2CMSST & 0x40))
        ;
    I2CMSST &= ~0x40;
}
void Start()
{
    I2CMSCR = 0x01; // Send START command
    Wait();
}
void SendData(char dat)
{
    I2CTXD = dat;   // Write data to the data buffer
    I2CMSCR = 0x02; // Send a SEND command
    Wait();
}
void RecvACK()
{
    I2CMSCR = 0x03; // Send read ACK command
    Wait();
}
char RecvData()
{
    I2CMSCR = 0x04; // Send RECV command
    Wait();
    return I2CRXD;
}
void SendACK()
{
    I2CMSST = 0x00; // Setup the ACK signal
    I2CMSCR = 0x05; // Send ACK command
    Wait();
}
void SendNAK()
{
    I2CMSST = 0x01; // Setup the NAK signal
    I2CMSCR = 0x05; // Send ACK command
    Wait();
}
void Stop()
{
    I2CMSCR = 0x06; // Send STOP command
    Wait();
}
void Delay()
{
    int i;
    for (i = 0; i < 3000; i++)
    {
        __asm NOP __endasm;
        __asm NOP __endasm;
        __asm NOP __endasm;
        __asm NOP __endasm;
    }
}

void delay1(uint16_t time)
{
    while (time--)
    {
        Delay();
    }
}

// Define I2C Address
#define OLED_ADDR 0x3C

// Define Commands
#define OLED_CMD_DISPLAY_OFF 0xAE
#define OLED_CMD_DISPLAY_ON 0xAF
#define OLED_CMD_SET_COL_ADDRESS 0x21
#define OLED_CMD_SET_PAGE_ADDRESS 0x22
#define OLED_CMD_SET_START_LINE 0x40
#define OLED_CMD_SET_CONTRAST 0x81

__xdata uint8_t display_buffer[512];

void oled_cmd(uint8_t data)
{
    Start();
    SendData(OLED_ADDR << 1);
    RecvACK();
    SendData(0x00);
    RecvACK();
    SendData(data);
    RecvACK();
    Stop();
}

void oled_data(uint8_t data)
{
    Start();
    SendData(OLED_ADDR << 1);
    RecvACK();
    SendData(0x40);
    RecvACK();
    SendData(data);
    RecvACK();
    Stop();
}

void oled_clear(uint8_t state)
{
    uint8_t page, col;
    for (page = 0; page < 4; page++)
    {
        oled_cmd(0xB0 + page);
        oled_cmd(0x00);
        oled_cmd(0x10);
        for (col = 0; col < 128; col++)
        {
            oled_data(state);
        }
    }
}

void oled_display()
{
    uint16_t posi = 0;
    uint8_t page, col;
    for (page = 0; page < 4; page++)
    {
        oled_cmd(0xB0 + page);
        oled_cmd(0x00);
        oled_cmd(0x10);
        for (col = 0; col < 128; col++)
        {
            oled_data(display_buffer[posi++]);
        }
    }
}

__xdata uint8_t cursor_x, cursor_y, textsize;
__xdata uint16_t textcolor, textbgcolor;

void drawPixel(int16_t x, int16_t y, uint16_t color)
{
    switch (color)
    {
    case 1:
        display_buffer[x + (y / 8) * 128] |= (1 << (y & 7));
        break;
    case 0:
        display_buffer[x + (y / 8) * 128] &= ~(1 << (y & 7));
        break;
    }
}

void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    while (h--)
    {
        drawPixel(x, y + h - 1, color);
    }
}

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    for (y = h; y > 0; y--)
    {
        for (x = w; x > 0; x--)
        {
            drawPixel(x + w - 1, y + h - 1, color);
        }
    }
}

void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size)
{
    for (int8_t i = 0; i < 5; i++)
    {
        uint8_t line = font[c * 5 + i];
        for (int8_t j = 0; j < 8; j++, line >>= 1)
        {
            if (line & 1)
            {
                if (size == 1)
                    drawPixel(x + i, y + j, color);
                else
                    fillRect(x + i * size, y + j * size, size, size, color);
            }
            else if (bg != color)
            {
                if (size == 1)
                    drawPixel(x + i, y + j, bg);
                else
                    fillRect(x + i * size, y + j * size, size, size, bg);
            }
        }
    }
    if (bg != color)
    {
        if (size == 1)
            drawFastVLine(x + 5, y, 8, bg);
        else
            fillRect(x + 5 * size, y, size, 8 * size, bg);
    }
}

void write(uint8_t c)
{
    if (c == '\n')
    {
        cursor_x = 0;
        cursor_y += textsize * 8;
    }
    else if (c != '\r')
    {
        /*if(((cursor_x + textsize * 6) > 240)) { // Off right?
            cursor_x  = 0;                 // Reset x to zero,
            cursor_y += textsize * 8;      // advance y one line
            }   */
        drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize);
        cursor_x += textsize * 6;
    }
}

void print(char text[])
{
    uint16_t poi = 0;
    while (text[poi] != 0)
    {
        write(text[poi++]);
    }
}

__xdata char str[50];
int8_t ddd = 0;
void main()
{
    P0M0 = 0x00;
    P0M1 = 0x00;
    P1M0 = 0x00;
    P1M1 = 0x00;
    P2M0 = 0x00;
    P2M1 = 0x00;
    P3M0 = 0x00;
    P3M1 = 0x00;

    P_SW2 = 0x90;
    I2CCFG = 0xe0; // Enable I2C master mode
    I2CMSST = 0x00;
    oled_cmd(OLED_CMD_DISPLAY_OFF);
    oled_cmd(0x00);
    oled_cmd(0x10);
    oled_cmd(0x40);
    oled_cmd(0xB0);
    oled_cmd(0x81);
    oled_cmd(0xC5);
    oled_cmd(0xA0);
    oled_cmd(0xA6);
    oled_cmd(0xA8);
    oled_cmd(0x1F);
    oled_cmd(0xC0);
    oled_cmd(0xD3);
    oled_cmd(0x30);
    oled_cmd(0xD5);
    oled_cmd(0x80);
    oled_cmd(0xD9);
    oled_cmd(0x22);
    oled_cmd(0xDA);
    oled_cmd(0x12);
    oled_cmd(0xDB);
    oled_cmd(0x20);
    oled_cmd(0x8D);
    oled_cmd(0x15);

    oled_clear(0x00);

    oled_cmd(OLED_CMD_DISPLAY_ON);

    textsize = 1;
    textcolor = 1, textbgcolor = 0;
    while (1 == 1)
    {
        if (P32 == 0)
            ddd++;
        else if (P30 == 0)
            ddd--;
        else if (P31 == 0)
            ddd = 0;
        cursor_x = 0;
        cursor_y = 0;
        for (uint16_t i = 0; i < 512; i++)
        {
            display_buffer[i] = 0x00;
        }
        sprintf(str, "Hi Twitter\nATC1441 : %i", ddd);
        print(str);
        oled_display();
        P34 = 1;
        delay1(1);
        P34 = 0;
        delay1(1);
    }
    P_SW2 = 0x00;
}