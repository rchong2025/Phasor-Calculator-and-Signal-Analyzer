#include <stdio.h>
#include <stdlib.h>
#include <EFM8LB1.h>
#include <string.h>

#define SYSCLK 72000000L
#define BAUDRATE 115200L
#define SARCLK 18000000L
#define CHARS_PER_LINE 16
#define NOISE 0.05

#define fabs(x)  ((x) < 0 ? -(x) : (x))

#define LCD_RS P1_7
// #define LCD_RW Px_x // Not used in this code.  Connect to GND
#define LCD_E  P2_0
#define LCD_D4 P1_3
#define LCD_D5 P1_2
#define LCD_D6 P1_1
#define LCD_D7 P1_0

#define ADD_BUTTON  P3_0
#define SUB_BUTTON  P2_6
#define MUL_BUTTON  P2_5
#define SAVE_BUTTON P2_4

#define DISPLAY_BUTTON P0_7

unsigned char overflow_count;

char _c51_external_startup (void)
{
	// Disable Watchdog with key sequence
	SFRPAGE = 0x00;
	WDTCN = 0xDE; //First key
	WDTCN = 0xAD; //Second key
  
	VDM0CN=0x80;       // enable VDD monitor
	RSTSRC=0x02|0x04;  // Enable reset on missing clock detector and VDD

	#if (SYSCLK == 48000000L)	
		SFRPAGE = 0x10;
		PFE0CN  = 0x10; // SYSCLK < 50 MHz.
		SFRPAGE = 0x00;
	#elif (SYSCLK == 72000000L)
		SFRPAGE = 0x10;
		PFE0CN  = 0x20; // SYSCLK < 75 MHz.
		SFRPAGE = 0x00;
	#endif
	
	#if (SYSCLK == 12250000L)
		CLKSEL = 0x10;
		CLKSEL = 0x10;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 24500000L)
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 48000000L)	
		// Before setting clock to 48 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x07;
		CLKSEL = 0x07;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 72000000L)
		// Before setting clock to 72 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x03;
		CLKSEL = 0x03;
		while ((CLKSEL & 0x80) == 0);
	#else
		#error SYSCLK must be either 12250000L, 24500000L, 48000000L, or 72000000L
	#endif
	
	P0MDOUT |= 0x10; // Enable UART0 TX as push-pull output
	P0MDOUT &= ~0b1000_0000;
	P2MDOUT &= ~0b0111_0000;
	P3MDOUT &= ~0b1000_1111;
	
	XBR0     = 0x01; // Enable UART0 on P0.4(TX) and P0.5(RX)
	XBR1     = 0X10; // Enable T0 on P0.0
	XBR2     = 0x40; // Enable crossbar and weak pull-ups                      

	// Configure Uart 0
	#if (((SYSCLK/BAUDRATE)/(2L*12L))>0xFFL)
		#error Timer 0 reload value is incorrect because (SYSCLK/BAUDRATE)/(2L*12L) > 0xFF
	#endif
	SCON0 = 0x10;
	TH1 = 0x100-((SYSCLK/BAUDRATE)/(2L*12L));
	TL1 = TH1;      // Init Timer1
	TMOD &= ~0xf0;  // TMOD: timer 1 in 8-bit auto-reload
	TMOD |=  0x20;                       
	TR1 = 1; // START Timer1
	TI = 1;  // Indicate TX0 ready
  	
	return 0;
}

void InitADC (void)
{
	SFRPAGE = 0x00;
	ADEN=0; // Disable ADC
	
	ADC0CN1=
		(0x2 << 6) | // 0x0: 10-bit, 0x1: 12-bit, 0x2: 14-bit
        (0x0 << 3) | // 0x0: No shift. 0x1: Shift right 1 bit. 0x2: Shift right 2 bits. 0x3: Shift right 3 bits.		
		(0x0 << 0) ; // Accumulate n conversions: 0x0: 1, 0x1:4, 0x2:8, 0x3:16, 0x4:32
	
	ADC0CF0=
	    ((SYSCLK/SARCLK) << 3) | // SAR Clock Divider. Max is 18MHz. Fsarclk = (Fadcclk) / (ADSC + 1)
		(0x0 << 2); // 0:SYSCLK ADCCLK = SYSCLK. 1:HFOSC0 ADCCLK = HFOSC0.
	
	ADC0CF1=
		(0 << 7)   | // 0: Disable low power mode. 1: Enable low power mode.
		(0x1E << 0); // Conversion Tracking Time. Tadtk = ADTK / (Fsarclk)
	
	ADC0CN0 =
		(0x0 << 7) | // ADEN. 0: Disable ADC0. 1: Enable ADC0.
		(0x0 << 6) | // IPOEN. 0: Keep ADC powered on when ADEN is 1. 1: Power down when ADC is idle.
		(0x0 << 5) | // ADINT. Set by hardware upon completion of a data conversion. Must be cleared by firmware.
		(0x0 << 4) | // ADBUSY. Writing 1 to this bit initiates an ADC conversion when ADCM = 000. This bit should not be polled to indicate when a conversion is complete. Instead, the ADINT bit should be used when polling for conversion completion.
		(0x0 << 3) | // ADWINT. Set by hardware when the contents of ADC0H:ADC0L fall within the window specified by ADC0GTH:ADC0GTL and ADC0LTH:ADC0LTL. Can trigger an interrupt. Must be cleared by firmware.
		(0x0 << 2) | // ADGN (Gain Control). 0x0: PGA gain=1. 0x1: PGA gain=0.75. 0x2: PGA gain=0.5. 0x3: PGA gain=0.25.
		(0x0 << 0) ; // TEMPE. 0: Disable the Temperature Sensor. 1: Enable the Temperature Sensor.

	ADC0CF2= 
		(0x0 << 7) | // GNDSL. 0: reference is the GND pin. 1: reference is the AGND pin.
		(0x1 << 5) | // REFSL. 0x0: VREF pin (external or on-chip). 0x1: VDD pin. 0x2: 1.8V. 0x3: internal voltage reference.
		(0x1F << 0); // ADPWR. Power Up Delay Time. Tpwrtime = ((4 * (ADPWR + 1)) + 2) / (Fadcclk)
	
	ADC0CN2 =
		(0x0 << 7) | // PACEN. 0x0: The ADC accumulator is over-written.  0x1: The ADC accumulator adds to results.
		(0x0 << 0) ; // ADCM. 0x0: ADBUSY, 0x1: TIMER0, 0x2: TIMER2, 0x3: TIMER3, 0x4: CNVSTR, 0x5: CEX5, 0x6: TIMER4, 0x7: TIMER5, 0x8: CLU0, 0x9: CLU1, 0xA: CLU2, 0xB: CLU3

	ADEN=1; // Enable ADC
}

void Init_Timer2(void) {

	SFRPAGE = 0x00;
	TMR2CN0 = 0x00;
	
	CKCON0 &= ~0x30;
	TMR2RL = 0x43EA;
	TMR2 = TMR2RL;
	
	IE &= ~0x20;
	TMR2CN0 |= 0x04;

}

// Uses Timer3 to delay <us> micro-seconds. 
void Timer3us(unsigned char us)
{
	unsigned char i;               // usec counter
	
	// The input for Timer 3 is selected as SYSCLK by setting T3ML (bit 6) of CKCON0:
	CKCON0|=0b_0100_0000;
	
	TMR3RL = (-(SYSCLK)/1000000L); // Set Timer3 to overflow in 1us.
	TMR3 = TMR3RL;                 // Initialize Timer3 for first overflow
	
	TMR3CN0 = 0x04;                 // Sart Timer3 and clear overflow flag
	for (i = 0; i < us; i++)       // Count <us> overflows
	{
		while (!(TMR3CN0 & 0x80));  // Wait for overflow
		TMR3CN0 &= ~(0x80);         // Clear overflow indicator
	}
	TMR3CN0 = 0 ;                   // Stop Timer3 and clear overflow flag
}

void waitms (unsigned int ms)
{
	unsigned int j;
	unsigned char k;
	for(j=0; j<ms; j++)
		for (k=0; k<4; k++) Timer3us(250);
}

#define VDD 3.3035 // The measured value of VDD in volts

void InitPinADC (unsigned char portno, unsigned char pinno)
{
	unsigned char mask;
	
	mask=1<<pinno;

	SFRPAGE = 0x20;
	switch (portno)
	{
		case 0:
			P0MDIN &= (~mask); // Set pin as analog input
			P0SKIP |= mask; // Skip Crossbar decoding for this pin
		break;
		case 1:
			P1MDIN &= (~mask); // Set pin as analog input
			P1SKIP |= mask; // Skip Crossbar decoding for this pin
		break;
		case 2:
			P2MDIN &= (~mask); // Set pin as analog input
			P2SKIP |= mask; // Skip Crossbar decoding for this pin
		break;
		default:
		break;
	}
	SFRPAGE = 0x00;
}

unsigned int ADC_at_Pin(unsigned char pin)
{
	ADC0MX = pin;   // Select input from pin
	ADINT = 0;
	ADBUSY = 1;     // Convert voltage at the pin
	while (!ADINT); // Wait for conversion to complete
	return (ADC0);
}

float Volts_at_Pin(unsigned char pin)
{
	 return ((ADC_at_Pin(pin)*VDD)/0b_0011_1111_1111_1111);
}

void LCD_pulse (void)
{
	LCD_E=1;
	Timer3us(40);
	LCD_E=0;
}

void LCD_byte (unsigned char x)
{
	// The accumulator in the C8051Fxxx is bit addressable!
	ACC=x; //Send high nible
	LCD_D7=ACC_7;
	LCD_D6=ACC_6;
	LCD_D5=ACC_5;
	LCD_D4=ACC_4;
	LCD_pulse();
	Timer3us(40);
	ACC=x; //Send low nible
	LCD_D7=ACC_3;
	LCD_D6=ACC_2;
	LCD_D5=ACC_1;
	LCD_D4=ACC_0;
	LCD_pulse();
}

void WriteData (unsigned char x)
{
	LCD_RS=1;
	LCD_byte(x);
	waitms(2);
}

void WriteCommand (unsigned char x)
{
	LCD_RS=0;
	LCD_byte(x);
	waitms(5);
}

void LCD_4BIT (void)
{
	LCD_E=0; // Resting state of LCD's enable is zero
	// LCD_RW=0; // We are only writing to the LCD in this program
	waitms(20);
	// First make sure the LCD is in 8-bit mode and then change to 4-bit mode
	WriteCommand(0x33);
	WriteCommand(0x33);
	WriteCommand(0x32); // Change to 4-bit mode

	// Configure the LCD
	WriteCommand(0x28);
	WriteCommand(0x0c);
	WriteCommand(0x01); // Clear screen command (takes some time)
	waitms(20); // Wait for clear screen command to finsih.
}

void LCDprint(char * string, unsigned char line, bit clear)
{
	int j;

	WriteCommand(line==2?0xc0:0x80);
	waitms(5);
	for(j=0; string[j]!=0; j++)	WriteData(string[j]);// Write the message
	if(clear) for(; j<CHARS_PER_LINE; j++) WriteData(' '); // Clear the rest of the line
}

float cos(float x) {
   // Constrain x to between -PI and PI
   while (x >  3.14159f) x -= 6.28318f; // 2 * PI
   while (x < -3.14159f) x += 6.28318f; // 2 * PI
   
   // Taylor series approximation
   return 1.0f - (x*x)/2.0f + (x*x*x*x)/24.0f - (x*x*x*x*x*x)/720.0f;
}

float sin(float x) {
    // sin(x) = cos(x - PI/2)
    return cos(x - 1.57079f); 
}

float sqrt(float x) {
    // Standard Newton-Raphson Method
    // Safe for ALL processors and 8051 memory architectures
    xdata float guess = 1.0f;
    xdata char k;
    
    if (x <= 0.0f) return 0.0f;
    
    // 8 iterations is plenty for 32-bit float accuracy
    for(k = 0; k < 8; k++) {
        guess = (guess + (x / guess)) / 2.0f;
    }
    
    return guess;
}

float atan(float x) {
    float x2 = x * x;
    return x * (1.0f - x2 * (0.3333f - x2 * (0.200f - x2 * (0.14285f - x2 * (0.1111f - x2 * 0.0909f)))));
}

float atan2(float y, float x) {
    float ratio;
    float result;

    if (x == 0.0f) {
        if (y > 0.0f) return  1.57079f; // PI / 2
        if (y < 0.0f) return -1.57079f; // -PI / 2
        return 0.0f;
    }

    ratio = y / x;

    if (ratio > 1.0f) {
        result =  1.57079f - atan(1.0f / ratio);
    } else if (ratio < -1.0f) {
        result = -1.57079f - atan(1.0f / ratio);
    } else {
        result = atan(ratio);
    }

    if (x < 0.0f) {
        if (y >= 0.0f) result += 3.14159f;
        else           result -= 3.14159f;
    }

    return result;
}

void main (void)
{
	xdata float v[2] = {0,0}, v_prev[2]= {0,0};
	xdata float period, p_prev, time_diff = 0;
	
	xdata float v_peak[2]= {0,0};
	
	xdata int flag = 0, display = 0;
	xdata int bus1 = 0, bus2 = 0;	//read busses 
	xdata float ref_RMS = 0, v_RMS = 0, phase = 0, freq = 0;
	
	xdata char line1[17];
	xdata char line2[17];
	
	xdata float v_save[4];
	xdata float phase_save[4];
	
	xdata float x1, x2, y1, y2, x_total, y_total, mag1, mag2, ang1, ang2;
	
    waitms(500); // Give PuTTy a chance to start before sending
	printf("\x1b[2J"); // Clear screen using ANSI escape sequence.
	
	printf ("ADC test program\n"
	        "File: %s\n"
	        "Compiled: %s, %s\n\n",
	        __FILE__, __DATE__, __TIME__);
	        
	LCDprint("          ", 1, 1);
	LCDprint("          ", 2, 1);
	
	
	InitPinADC(2, 2); // Configure P2.2 as analog input
	InitPinADC(2, 3); // Configure P2.3 as analog input
    InitADC();
    Init_Timer2();

	while(1)
	{	
		v[0] = Volts_at_Pin(QFP32_MUX_P2_2);
		v[1] = Volts_at_Pin(QFP32_MUX_P2_3);
		
		if (v[0] <= 0.001 && v_prev[0] > 0.001 && flag == 0) {
			TMR2CN0 |= 0x04;
			flag = 1;
		}
		else if (v[0] > 0.001 && v_prev[0] <= 0.001 && flag == 1) {
			TMR2CN0 &= ~0x04;
			period = (2*TMR2) * (1.23274 / 7500000.0);
		    phase = 360.0f*time_diff/period;
		    freq = 1/period;		
		
			flag = 2;
			TMR2 = 0;		
		}
		
		else if (v[0] <= 0.001 && v_prev[0] > 0.001 && flag == 2) {
			TMR2CN0 |= 0x04;
			flag = 3;
		}
		else if (v[1] <= 0.001 && v_prev[1] > 0.001 && flag == 3) {
			TMR2CN0 &= ~0x04;
			time_diff = (TMR2) * (1.23274 / 7500000.0);
			flag = 4;
			TMR2 = 0;	
		}
		else if (flag == 4) {
		
			flag = 0;
						
			printf("\x1b[2J\x1b[H"); // ANSI: Clear from cursor to end of line.

			if(display == 1) {
				sprintf(line1, "v:%3.2f p:%3.2f %i", v_save[bus1], phase_save[bus1], bus1);
				sprintf(line2, "v:%3.2f p:%3.2f %i", v_save[bus2], phase_save[bus2], bus2);
			    printf("Read bus 1: %f, %f\nRead bus 2: %f, %f \n", v_save[bus1], phase_save[bus1], v_save[bus2], phase_save[bus2]);
			} else {
				sprintf(line1, "r:%3.2f v:%3.2f", ref_RMS, v_RMS);
				sprintf(line2, "p:%3.2f f:%3.2f", phase, freq);
				printf("Reference amplitude: %f \nVoltage amplitude: %f \nPhase: %f \nfrequency: %f\n", ref_RMS, v_RMS, phase, freq);
			}
			LCDprint(line1, 1, 1);
			LCDprint(line2, 2, 1);	
		
		}
		
		if (v[0] > v_peak[0] && v[0] < 10) v_peak[0] = v[0];
		if (v[1] > v_peak[1] && v[1] < 10) v_peak[1] = v[1];
		
		v_peak[0] *= 0.9999f;
		v_peak[1] *= 0.9999f;
				
		ref_RMS = 2*v_peak[0]/1.41421356f;
		v_RMS = 2*v_peak[1]/1.41421356f;
		
		v_prev[0] = v[0];
		v_prev[1] = v[1]; 
		p_prev = period;
		
		if (P3_7 && P3_3) bus1 = 3;
		else if (P3_7) bus1 = 2;
		else if (P3_3) bus1 = 1;
		else bus1 = 0;
		
		if (P3_2 && P3_1) bus2 = 3;
		else if (P3_2) bus2 = 2;
		else if (P3_1) bus2 = 1;
		else bus2 = 0;
		
		if (DISPLAY_BUTTON == 0) {
			waitms(20);
			if (DISPLAY_BUTTON == 0) {
			
				if (display == 1) display = 0;
				else display = 1;
				
				while (DISPLAY_BUTTON == 0);
				waitms(20);
			}
		}
		
		// ADD (Bus1 = Bus1 + Bus2)
        if (ADD_BUTTON == 0) {
            waitms(20);
            if (ADD_BUTTON == 0) {

                mag1 = v_save[bus1];
                ang1 = phase_save[bus1] * 3.14f/180.0f; // Deg -> Rad

                mag2 = v_save[bus2];
                ang2 = phase_save[bus2] * 3.14f/180.0f; // Deg -> Rad

                x1 = mag1 * cos(ang1);
                y1 = mag1 * sin(ang1);
                x2 = mag2 * cos(ang2);
                y2 = mag2 * sin(ang2);

                x_total = x1 + x2;
                y_total = y1 + y2;

                // Save result back to Bus 1 (Accumulator style)
                v_save[bus1] = sqrt((x_total * x_total) + (y_total * y_total));
                phase_save[bus1] = atan2(y_total, x_total) * 180.0f/3.14f; // Rad -> Deg

                if (phase_save[bus1] < 0.0f) phase_save[bus1] += 360.0f;

                while (ADD_BUTTON == 0);
                waitms(20);
            }
        }
		
		// SUBTRACT (Bus1 = Bus1 - Bus2)
        if (SUB_BUTTON == 0) {
            waitms(20);
            if (SUB_BUTTON == 0) {

                mag1 = v_save[bus1];
                ang1 = phase_save[bus1] * 3.14f/180.0f;

                mag2 = v_save[bus2];
                ang2 = phase_save[bus2] * 3.14f/180.0f;

                x1 = mag1 * cos(ang1);
                y1 = mag1 * sin(ang1);
                x2 = mag2 * cos(ang2);
                y2 = mag2 * sin(ang2);

                x_total = x1 - x2;
                y_total = y1 - y2;

                v_save[bus1] = sqrt((x_total * x_total) + (y_total * y_total));

                // FIXED: Multiplied by 180/PI to get degrees
                phase_save[bus1] = atan2(y_total, x_total) * 180.0f/3.14f;

                if (phase_save[bus1] < 0.0f) phase_save[bus1] += 360.0f;

                display = 1;
                while (SUB_BUTTON == 0);
                waitms(20);
            }
        }
		
		if (MUL_BUTTON == 0) {
			waitms(20);
			if (MUL_BUTTON == 0) {
			
				v_save[bus1] = v_save[bus1] * v_save[bus2];        // magnitudes multiply
				
                phase_save[bus1] = phase_save[bus1] + phase_save[bus2]; // phases add
				
				while (MUL_BUTTON == 0);
				waitms(20);
			}
		}
		
		if (SAVE_BUTTON == 0) {
			waitms(20);
			if (SAVE_BUTTON == 0) {
			
				v_save[bus1] = v_RMS;
				phase_save[bus1] = phase;
				
				while (SAVE_BUTTON == 0);
				waitms(20);
			}
		}

	 }  
}	