/****************************************************************************/
/* Source Code for Brittany and Kenji's October 13th, 2023 Lantern Project  */
/****************************************************************************/
#include <msp430.h>
#include <stdint.h>

#define selections 48   // One less than possible selections since 0 indexed
#define duties 5        // One less than possible selections since 0 indexed
#define setRGBW 8       // Maximum RGBW intensity
#define cButton 2048    // Check button status after this many cycles

// 1st Button click - Change duty cycle
// 2nd Button click - Read jumpers
uint8_t BTN1 = 0;       // Used to track if looking for high/low button state
uint8_t BTN2 = 0;       // Used to track if looking for high/low button state

uint8_t duty = 3;       // Used to track the current dutyCycle
uint8_t selected = 255; // Used to track the current color selection
uint8_t flagStatic = 1; // 0 if color changes, 1 if fixed
// Using software PWM since device doesn't have enough timer flags
uint8_t setR = 0;
uint8_t setG = 0;
uint8_t setB = 0;
uint8_t setW = 0;
uint8_t cntRGBW = setRGBW;

uint8_t flagDuty = 0;
uint8_t cntDuty = 0;
uint8_t setDuty = 0;
const uint8_t dutyCycles[duties+1] = {0,4,16,32,64,128}; // Change the amount of dimming here

// RGB in 8-bits, use as count of time "on" during a "0 to 255" unit period
#define cW1 2
#define cW3 24
#define cW6 48
uint8_t pIdx = 0;
uint8_t pIter;
uint16_t cntPause = 1;
// Red 0, orange 1, yellow 2, light green 3, green 4,
// blue 5, dark blue 6, purple 7, pink 8
const uint8_t cntClookup[9] = {0, 3, 7, 14, 16, 20, 32, 36, 45};
const uint16_t pauseHigh[6] = {32768,32768,4096,8192,8192,8192};
const uint8_t cWheel1[cW1] = {0,16}; // Red-Green
const uint8_t cWheel2[cW1] = {36,4}; // Purple-Orange
const uint8_t cWheel3[cW3] = {21,22,23,24,25,26,27,28,29,30,31,32,32,31,30,29,28,27,26,25,24,23,22,21}; // Blues
const uint8_t cWheel4[cW3] = {15,16,17,18,19,20,23,26,29,32,35,36,37,36,33,30,27,24,21,19,18,17,15,14}; // Cool (Blue, Green, Purple)
const uint8_t cWheel5[cW3] = {0,0,1,2,2,3,4,5,6,7,8,9,10,10,9,8,7,6,5,4,3,2,1,1}; // Warm (Red, Orange, Yellow)
const uint8_t cWheel6[cW6+2] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49}; // Fine Rainbow and 
const uint8_t *cWheel;
uint8_t setC;
uint8_t cntC = 0;
// Could also do this programmatically and save a bunch of memory
const uint8_t colors[selections+2][4] = { // 0 is always on 15 is max static ratio to allow for 16 duty cycles
               {8,0,0,0}, // Red 0
               {8,1,0,0}, //
               {8,2,0,0}, //
               {8,3,0,0}, //
               {8,4,0,1}, // Orange 4
               {8,5,0,1}, //
               {8,6,0,2}, //
               {8,7,0,2}, //
               {8,8,0,3}, // Yellow 8
               {7,8,0,2}, //
               {6,8,0,1}, //
               {5,8,0,0}, //
               {4,8,0,0}, // Lime Green 12
               {3,8,0,0}, //
               {2,8,0,0}, //
               {1,8,0,0}, //
               {0,8,0,0}, // Green 16
               {0,8,1,0}, //
               {0,8,2,0}, //
               {0,8,3,0}, //
               {0,8,4,0}, // Teal 20
               {0,8,5,0}, //
               {0,8,6,0}, //
               {0,8,7,0}, //
               {0,8,8,0}, // Cyan 24
               {0,7,8,0}, //
               {0,6,8,0}, //
               {0,5,8,0}, //
               {0,4,8,0}, // Sky Blue 28
               {0,3,8,0}, //
               {0,2,8,0}, //
               {0,1,8,0}, //
               {0,0,8,0}, // Blue 32
               {1,0,8,0}, //
               {2,0,8,0}, //
               {3,0,8,0}, //
               {4,0,8,0}, // Purple 36
               {5,0,8,0}, //
               {6,0,8,0}, //
               {7,0,8,0}, //
               {8,0,8,0}, // Magenta 40
               {8,0,7,0}, //
               {8,0,6,0}, //
               {8,0,5,0}, //
               {8,0,4,0}, // Rose 44
               {8,0,3,0}, //
               {8,0,2,0}, //
               {8,0,1,0}, //
               {0,0,0,8}, // White
               {8,8,8,8}  // All LEDS
};


int main(void)
{
    duty = 3;                       // Don't start at max brightness
    setDuty = dutyCycles[duty];
    WDTCTL = WDTPW | WDTHOLD;       // Stop WDT
    // Update the operating clocks
    FRCTL0 = FRCTLPW | NWAITS_1;    // FRAM Wait required for DCO 8MHz+
    CSCTL0 = 0x00;                  // Clear DCO and MOD bits
    CSCTL1 = DCORSEL_1;             // select DCO to run at 2MHz.

    // P1.6/P2.1 RED            // P1.7/P2.0 GREEN
    // P1.3/P1.0 WHITE/YELLOW   // P1.2/P1.1 BLUE
    // P1.5 SW3 BTN1            // P1.4 SW2 BTN2
    // P1.6 Jumper 1            // P1.7 Jumper 2
    // P1.2 Jumper 3            // P1.3 Jumper 4
//    P1REN = 0x00;    // Disable internal resistance
//    P2REN = 0x00;    // Disable internal resistance
    P1DIR |= (BIT0 | BIT1);     // Set White/Yellow and Blue as output
    P2DIR |= (BIT0 | BIT1);     // Set Red and Green as output
    P1OUT = 0x00;
    P2OUT = 0x00;
    // Disable the GPIO power-on default high-impedance mode to use port settings
    PM5CTL0 &= ~LOCKLPM5;

    // Start with a defined color (WHITE/YELLOW)
    setR = 0;
    setG = 0;
    setB = 0;
    setW = 8;

    TB0CCTL0 |= CCIE;                           // TBCCR0 interrupt enabled
    TB0CCR0 = cButton;                          // Check if button press
    TB0CTL |= TBSSEL__SMCLK | MC__CONTINUOUS;   // SMCLK, continuous mode
    __bis_SR_register(GIE);                     // Enable interrupts

    while (1){
        cntRGBW--;
        if (!flagStatic){cntPause--;}
        if (cntRGBW == 0){ // Turn LEDs off for duty cycling
            if (cntDuty == 0){
                if (setR != setRGBW){P2OUT &= ~BIT1;}
                if (setG != setRGBW){P2OUT &= ~BIT0;}
                if (setB != setRGBW){P1OUT &= ~BIT1;}
                if (setW != setRGBW){P1OUT &= ~BIT0;}
                if ((cntPause > 0) & !flagStatic){cntPause--;}
            }
            while (cntDuty > 0){
                P2OUT &= ~BIT1;
                P2OUT &= ~BIT0;
                P1OUT &= ~BIT0;
                P1OUT &= ~BIT1;
                cntDuty--;
                if ((cntPause > 0) & !flagStatic){cntPause--;}
                __delay_cycles(20);
            }
            cntDuty = setDuty;
            cntRGBW = setRGBW;
        } else {// Turn on LEDs according to set point
            if (cntRGBW <= setR){P2OUT |= BIT1;}
            if (cntRGBW <= setG){P2OUT |= BIT0;}
            if (cntRGBW <= setB){P1OUT |= BIT1;}
            if (cntRGBW <= setW){P1OUT |= BIT0;}
        }
        if (cntPause == 0) {
            cntC++; // Next color index
            if (cntC>=setC){cntC = 0;}
            setR = colors[cWheel[cntC]][0];
            setG = colors[cWheel[cntC]][1];
            setB = colors[cWheel[cntC]][2];
            setW = colors[cWheel[cntC]][3];
            cntPause = pauseHigh[pIdx];
        }
    } // while (1)
}


// Timer_B0 interrupt service routine
#pragma vector=TIMER0_B0_VECTOR
__interrupt void TIMER0_B0_ISR (void)
{
    // Pins used for button don't support interrupts, so do this in software
    uint8_t BUTTON = P1IN;
    if (BTN2){ // A BTN was pushed
        if ((BUTTON & BIT5) == 0){ // The push is released
            BTN2 = 0;
        }
    } else{
        if (BUTTON & BIT5){ // A BTN2 push is initialized
            BTN2 = 1;
            // Cycle the duty cycle up or down
            if (flagDuty){
                duty--;
                if (duty > duties){duty = 0;}
                if (duty == 0){flagDuty = 0;}
            } else{
                duty++;
                if (duty > duties){duty = duties;}
                if (duty == duties){flagDuty = 1;}
            }
            setDuty = dutyCycles[duty];
        }
    }
    if (BTN1){ // A BTN was pushed
        if ((BUTTON & BIT4) == 0){ // The push is released
            BTN1 = 0;
        }
    } else{
        if (BUTTON & BIT4){ // A BTN1 push is initialized
            BTN1 = 1;
            BUTTON = 0;                 // Variable that will store jumper state
            // Turn off LEDs (i.e. disable outputs)
            P1DIR = 0x00;
            P2DIR = 0x00;
            __delay_cycles(5000);       // 2.5ms @ 2MHz clock
            // Read jumper values
            uint8_t JUMPER1 = P1IN;
            uint8_t JUMPER2 = P2IN;
            // Re-enable LEDs
            P1DIR |= (BIT0 | BIT1);     // Set White/Yellow and Blue as output
            P2DIR |= (BIT0 | BIT1);     // Set Red and Green as output
            if (JUMPER1 & BIT0){BUTTON += 1;}
            if (JUMPER1 & BIT1){BUTTON += 2;}
            if (JUMPER2 & BIT0){BUTTON += 4;}
            if (JUMPER2 & BIT1){BUTTON += 8;}
            cntPause = 16;
            cWheel = &cWheel6[0];       // To allow default selection from entire rainbow
            if (BUTTON != 0){           // if Jumpers are all zero, just increment last selection
                selected = BUTTON;
                cntRGBW = 1;
                flagStatic = 1;
                cntC = cntClookup[selected-1];
                setC = cW3;             // Uses a bit less memory than reusing in switch
                switch (selected){
                    case 10:
                        cWheel = &cWheel1[0];
                        setC = cW1;
                        break;
                    case 11:
                        cWheel = &cWheel2[0];
                        setC = cW1;
                        break;
                    case 12:
                        cWheel = &cWheel3[0];
                        break;
                    case 13:
                        cWheel = &cWheel4[0];
//                        setC = cW3;
                        break;
                    case 14:
                        cWheel = &cWheel5[0];
//                        setC = cW3;
                        break;
                    case 15:
                        cWheel = &cWheel6[0];
                        setC = cW6;
                        break;
                }
                if (selected >= 10){    // Higher selections will flash a pattern
                    cntC = 0;
                    flagStatic = 0;
                    pIdx = selected-10;
                }
                // Copy the configuration to set registers
                setR = colors[cWheel[cntC]][0];
                setG = colors[cWheel[cntC]][1];
                setB = colors[cWheel[cntC]][2];
                setW = colors[cWheel[cntC]][3];
            } else{
                flagStatic = 1;         // A low configuration does not flash
                selected++;             // Increment through the possible selections
                if (selected > selections+2){selected = 0;} // +1 allows pure white, +2 for all lights
                // Copy the configuration to set registers
                setR = colors[cWheel[selected]][0];
                setG = colors[cWheel[selected]][1];
                setB = colors[cWheel[selected]][2];
                setW = colors[cWheel[selected]][3];
            }
        }
    }
    TB0CCR0 += cButton; // Add Offset to TBCCR0 to schedule next check
}
