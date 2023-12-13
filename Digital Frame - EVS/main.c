/* finalFinal.c
* Modified by: Erik van Schijndel
*/
#include <msp430.h>
#include "msp430fr6989.h"
#include "Grlib/grlib/grlib.h"          // Graphics library (grlib)
#include "LcdDriver/lcd_driver.h"       // LCD driver
#include <stdio.h>

#include "rachel.c"             //Image files for display
#include "EA.c"
#include "ENR.c"
#include "erik.c"
#include "erik2.c"

#define S1 BIT1     //Defining the left button to enter the photos
#define S2 BIT2     //Defining the right button to return to homescreen

Graphics_Context g_sContext;        // defining drawing contexts to be used to draw onto the screen
Graphics_Context ece_sContext;
int pictureCount = 1;           //global variable containing the picture currently selected
volatile unsigned int input = 2; //input 1: S1; input 0: S2; input 2:

const tImage  rachel8BPP_UNCOMP= // rachel.c, image 1
{
    IMAGE_FMT_8BPP_UNCOMP,
    128,
    128,
    256,
    palette_rachel8BPP_UNCOMP,
    pixel_rachel8BPP_UNCOMP,
};

const tImage  IMG_66758BPP_UNCOMP= // erik2.c image 2
{
    IMAGE_FMT_8BPP_UNCOMP,
    128,
    128,
    256,
    palette_IMG_66758BPP_UNCOMP,
    pixel_IMG_66758BPP_UNCOMP,
};

const tImage  IMG_33218BPP_UNCOMP= // erik.c image 3
{
    IMAGE_FMT_8BPP_UNCOMP,
    128,
    128,
    256,
    palette_IMG_33218BPP_UNCOMP,
    pixel_IMG_33218BPP_UNCOMP,
};

const tImage  IMG_32838BPP_UNCOMP= // ENR.c image 4
{
    IMAGE_FMT_8BPP_UNCOMP,
    128,
    128,
    256,
    palette_IMG_32838BPP_UNCOMP,
    pixel_IMG_32838BPP_UNCOMP,
};

const tImage  IMG_45728BPP_UNCOMP= // EA image 5
{
    IMAGE_FMT_8BPP_UNCOMP,
    128,
    128,
    256,
    palette_IMG_45728BPP_UNCOMP,
    pixel_IMG_45728BPP_UNCOMP,
};


    /*
    * A10 and A8 configuration for JOYSTICK
    */
void config_ADC_JOYSTICK(){
    // Enable Pins
    // Horizontal: P9.2 --> A10 to Digital
    P9SEL1 |= BIT2;
    P9SEL0 |= BIT2;
    // Vertical: P8.7 --> A4 to Digital
    P8SEL1 |= BIT7;
    P8SEL0 |= BIT7;

    ADC12CTL0 |= ADC12ON; //ADC enable
    ADC12CTL0 &= ~ADC12ENC; //Turn-off ENC (Enable Conversion) at first

    //Setup ADC12CTL0
    ADC12CTL0|=ADC12ON|ADC12SHT1_10; //ADC12SHT0: control the interval of the sampling timer (the number of cycles)

    //Setup ADC12CTL1
    // ADC12SHS: configure ADC12SC bit as the trigger signal
    // ADC12SHP ADC12 Sample/Hold Pulse Mode
    // ADC12DIV ADC12 Clock Divider Select: 7
    // ADC12SSEL ADC12 Clock Source Select: 0
    ADC12CTL1= ADC12SHS_0|ADC12SHP|ADC12DIV_7|ADC12SSEL_0;
    ADC12CTL1|=ADC12CONSEQ_1;

    //Setup ADC12CTL2
    // ADC12RES: 12-bit resolution
    // ADC12DF: unsigned bin-format
    ADC12CTL2|= ADC12ENC;

    //Setup ADC12MCTL0
    // ADC12VRSEL: VR+=AVCC, VR-=AVSS
    // ADC12INCH: Channel A10
    ADC12MCTL0|= ADC12INCH_10|ADC12VRSEL_0;
    // ADC12INCH: Channel A4
    ADC12MCTL1|=ADC12INCH_4|ADC12VRSEL_0|ADC12EOS;

    ADC12CTL0 |= ADC12ENC; //Turn-on ENC at the end
    return;
}
/*
* GPIO Initialization
*/
void config_LCD(){
    PJSEL0 = BIT4 | BIT5; //LFXT

    // Initialize LCD segments
    LCDCPCTL0 = 0xFFFF;
    LCDCPCTL1 = 0xFC3F;
    LCDCPCTL2 = 0x0FFF;

    // Configure LFXT 32kHz crystal
    CSCTL0_H = CSKEY >> 8;
    CSCTL4 &= ~LFXTOFF; // Enable LFXT

    do {
    CSCTL5 &= ~LFXTOFFG; // Clear LFXT fault flag
    SFRIFG1 &= ~OFIFG;
    }while (SFRIFG1 & OFIFG); // Test oscillator fault flag

    CSCTL0_H = 0; // Lock CS registers

    // Initialize LCD_C
    // ACLK, Divider = 1, Pre-divider = 16; 4-pin MUX
    LCDCCTL0 = LCDDIV__1 | LCDPRE__16 | LCD4MUX | LCDLP;
    // VLCD generated internally,
    // V2-V4 generated internally, v5 to ground
    // Set VLCD voltage to 2.60v
    // Enable charge pump and select internal reference for it
    LCDCVCTL = VLCD_1 | VLCDREF_0 | LCDCPEN;
    LCDCCPCTL = LCDCPCLKSYNC; // Clock synchronization enabled
    LCDCMEMCTL = LCDCLRM; // Clear LCD memory
    //Turn LCD on
    LCDCCTL0 |= LCDON;

    return;
}

/*
* We configure ACLK to 32 KHz
* The default value of ACLK is 39KHz
* */
void config_ACLK_to_32KHz(){
    //Perform the LFXIN and LFXOUT functionality
    PJSEL1 &= ~BIT4;
    PJSEL0 |= BIT4;

    CSCTL0 = CSKEY; // Unlock CS registers
    do{
        CSCTL5 &= ~LFXTOFFG; // Local fault flag
        SFRIFG1 &= ~OFIFG; // Global fault flag
    } while((CSCTL5 & LFXTOFFG) != 0);

    CSCTL0_H = 0; // Lock CS registers
    return;
}

/**
* main.c
*/
int main(void)
{
    WDTCTL = WDTPW | WDTHOLD; //Stop Watchdog Timer
    PM5CTL0 &= ~LOCKLPM5; //Disable GPIO power-on default high impedance mode

    //Buttons, inputs
    P1DIR &= ~S1;               //Set P1.1 as input
    P1IE |= BIT1;               //Enabling the interrupt bit
    P1IFG &= ~BIT1;             //Clear the flag
    P1REN |= S1;                //Enable built-in resistor
    P1OUT |= S1;                //Set the p1.1 as pull-up
    P1DIR &= ~S2;               //Set P1.2 as input
    P1IE |= BIT2;               //Enabling the interrupt bit
    P1IFG &= ~BIT2;             //Clearing the flag
    P1REN |= S2;                //Enabling built-in resistor
    P1OUT |= S2;                //Set the p1.2 as pull-up
    P1IFG &= ~BIT1;             //Clear the P1.1 flag
    P1IFG &= ~BIT2;             // Clear the P1.2 Flag
    _enable_interrupts();       //Enable the GIE

    //Begin to set Timer_A
    // Selected ACLK
    // Divide timer by 2
    // Up mode
    // TAR Clear
    TA0CTL = TASSEL_1 | ID_1 | MC_1 | TACLR;

    TA0CCR0 = 16000; // Up counter set to 16000, as 32 KHz / 2 will equal 16000 for a 1 second delay
    TA0CTL &= ~TAIFG;

    // LCD Config: specify SMCLK to 8 MHz in SPI clock
    CSCTL0 = CSKEY;                 // Unlock CS registers
    CSCTL3 &= ~(BIT4|BIT5|BIT6);    // DIVS=0
    CSCTL0_H = 0;                   // Relock the CS registers

    Crystalfontz128x128_Init();         // Initializes the display driver
    Crystalfontz128x128_SetOrientation(0); // Set the LCD orientation
    Graphics_initContext(&g_sContext, &g_sCrystalfontz128x128); // Initialize a drawing context
    Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK); // Set the background color to black
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE); // Set the foreground color to white
    GrContextFontSet(&g_sContext, &g_sFontFixed6x8); //Sets the font to be used

    Graphics_initContext(&ece_sContext, &g_sCrystalfontz128x128); // Initialize the context for our second frame (pictures)
    Graphics_setBackgroundColor(&ece_sContext, GRAPHICS_COLOR_BLACK); // Set background color to black
    Graphics_setForegroundColor(&ece_sContext, GRAPHICS_COLOR_WHITE); // Set foreground color to white
    GrContextFontSet(&ece_sContext, &g_sFontFixed6x8); // Set the default font for strings

    // Functions calls
    config_ADC_JOYSTICK();
    firstFrame(); // Populates screen on start


    for(;;) {
        ADC12CTL0|=ADC12SC;
        while ((ADC12CTL1 & ADC12BUSY)==0);

        if(ADC12MEM0 <= 10) // left
        {
            while((TA0CTL & TAIFG) == 1){ // Waiting for  1 second delay from timer
                if(pictureCount >= 1 && pictureCount <= 5){
                    pictureCount = pictureCount - 1; // Decrementing secFrame
                }
                else if(pictureCount < 1){
                    pictureCount = 5;
                }
                secFrame(pictureCount); // Calls secFrame with updated picture count
                TA0CTL &= ~TAIFG; // Clear timer flag
            }
        }
        if(3980 <= ADC12MEM0) // Joystick right
        {
            while((TA0CTL & TAIFG) == 1){ // Waiting for 1 second delay from timer
            if(pictureCount >= 1 && pictureCount <= 5){
                pictureCount = pictureCount + 1; // Incrementing secFrame
            }
            else if(pictureCount > 5){
                pictureCount = 1;
            }
            secFrame(pictureCount); // Calls secFrame with updated picture count
            TA0CTL &= ~TAIFG; // Clear timer flag
            }
        }
    }
    return 0;
}

void secFrame(unsigned int pictureCount){ // Contains image information for image frames
    if(pictureCount == 1){
        Graphics_drawImage(&g_sContext, &rachel8BPP_UNCOMP, 0, 0); // rachel.c
    }
    else if(pictureCount == 2){
        Graphics_drawImage(&g_sContext, &IMG_66758BPP_UNCOMP, 0, 0); // erik2.c
    }
    else if(pictureCount == 3){
        Graphics_drawImage(&g_sContext, &IMG_33218BPP_UNCOMP, 0, 0); // erik.c
    }
    else if(pictureCount == 4){
        Graphics_drawImage(&g_sContext, &IMG_32838BPP_UNCOMP, 0, 0); // ENR.c
    }
    else if(pictureCount == 5){
        Graphics_drawImage(&g_sContext, &IMG_45728BPP_UNCOMP, 0, 0); // EA.c
    }
}

void firstFrame(){ // Initial screen, shows user controls.
    Graphics_clearDisplay(&ece_sContext); // Clear display
    Graphics_drawStringCentered(&g_sContext, "ECE 431 Photo Frame", AUTO_STRING_LENGTH, 64, 30, OPAQUE_TEXT); // drawStrings to print text on home screen
    Graphics_drawStringCentered(&g_sContext, "Developed by:", AUTO_STRING_LENGTH, 64, 45, OPAQUE_TEXT);
    Graphics_drawStringCentered(&g_sContext, "Erik Van Schijndel", AUTO_STRING_LENGTH, 64, 60, OPAQUE_TEXT);
    Graphics_drawStringCentered(&g_sContext, ">>Press S1 to start", AUTO_STRING_LENGTH, 64, 75, OPAQUE_TEXT);
    Graphics_drawStringCentered(&g_sContext, ">>Press S2 to back", AUTO_STRING_LENGTH, 64, 90, OPAQUE_TEXT);
}

#pragma vector = PORT1_VECTOR // ISR for our S1 and S2 buttons in order to switch between frames.
__interrupt void ISR_Final_S1() {
    if( (P1IFG & BIT1) != 0 ){ // Checks for being on firstFrame, and sets user to secFrame.
        secFrame(1); // Image screen
        P1IFG &= ~BIT1; // Reset flag
    }else if((P1IFG & BIT2) != 0){ // Sets user to home page after selecting S2
        firstFrame(); // Home screen
        P1IFG &= ~BIT2; // Reset flag
    }
}
