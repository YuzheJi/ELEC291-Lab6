#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include "../Common/Include/stm32l051xx.h"
#include "lcd.h"


#define F_CPU 32000000L
#define factor 7312.5


void delay(int dly)
{
	while( dly--);
}

void Configure_Pins (void)
{
	RCC->IOPENR |= BIT0; // peripheral clock enable for port A
	
	// Make pins PA0 to PA5 outputs (page 200 of RM0451, two bits used to configure: bit0=1, bit1=0)
    GPIOA->MODER = (GPIOA->MODER & ~(BIT0|BIT1)) | BIT0; // PA0
	GPIOA->OTYPER &= ~BIT0; // Push-pull
    
    GPIOA->MODER = (GPIOA->MODER & ~(BIT2|BIT3)) | BIT2; // PA1
	GPIOA->OTYPER &= ~BIT1; // Push-pull
    
    GPIOA->MODER = (GPIOA->MODER & ~(BIT4|BIT5)) | BIT4; // PA2
	GPIOA->OTYPER &= ~BIT2; // Push-pull
    
    GPIOA->MODER = (GPIOA->MODER & ~(BIT6|BIT7)) | BIT6; // PA3
	GPIOA->OTYPER &= ~BIT3; // Push-pull
    
    GPIOA->MODER = (GPIOA->MODER & ~(BIT8|BIT9)) | BIT8; // PA4
	GPIOA->OTYPER &= ~BIT4; // Push-pull
    
    GPIOA->MODER = (GPIOA->MODER & ~(BIT10|BIT11)) | BIT10; // PA5
	GPIOA->OTYPER &= ~BIT5; // Push-pull
}

void wait_1ms(void)
{
	// For SysTick info check the STM32L0xxx Cortex-M0 programming manual page 85.
	SysTick->LOAD = (F_CPU/1000L) - 1;  // set reload register, counter rolls over from zero, hence -1
	SysTick->VAL = 0; // load the SysTick counter
	SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; // Enable SysTick IRQ and SysTick Timer */
	while((SysTick->CTRL & BIT16)==0); // Bit 16 is the COUNTFLAG.  True when counter rolls over from zero.
	SysTick->CTRL = 0x00; // Disable Systick counter
}

void waitms(int len)
{
	while(len--) wait_1ms();
}
// mode: 0 - 00 no pull, 1 - 01 pull up, 2 - 10 pull down
void Set_Pin_Input(int pin, int pmode){
	uint32_t upper_bits, bottom_bits; 
	upper_bits = 1UL << pin * 2 + 1; 
	bottom_bits = 1UL << pin * 2; 

	GPIOA->MODER &= ~(bottom_bits | upper_bits);
	
	if (pmode == 0){
		GPIOA->PUPDR &= ~(bottom_bits);
		GPIOA->PUPDR &= ~(upper_bits);
	}
	else if (pmode == 1){
		GPIOA->PUPDR |= (bottom_bits);
		GPIOA->PUPDR &= ~(upper_bits);
	}
	else if (pmode == 2){
		GPIOA->PUPDR |= (bottom_bits);
		GPIOA->PUPDR |= (upper_bits);
	}
}
// 0 - Bank A, 1 - Bank B
void Set_Pin_Output(int bank, int pin, int pmode){
	uint32_t upper_bits, bottom_bits; 
	uint16_t type_bits; 
	type_bits = 1UL << pin; 
	upper_bits = 1UL << (pin * 2 + 1); 
	bottom_bits = 1UL << (pin * 2); 
	
	if (bank == 0){
		GPIOA->MODER = (GPIOA->MODER & ~(upper_bits|bottom_bits)) | bottom_bits;
		if (pmode == 0){
			GPIOA->OTYPER &= ~(type_bits);
		}	
	}
	else if (bank == 1){
		GPIOB->MODER = (GPIOB->MODER & ~(upper_bits|bottom_bits)) | bottom_bits;
		if (pmode == 0){
			GPIOB->OTYPER &= ~(type_bits);
		}
	}
}


#define PIN_PERIOD (GPIOA->IDR&BIT8)
#define Button1 (GPIOA->IDR&BIT15) // PA15
#define Button2 (GPIOA->IDR&BIT14) // PA14
#define Button3 (GPIOA->IDR&BIT13) // PA13
#define Button4 (GPIOA->IDR&BIT12) // PA12
#define Button5 (GPIOA->IDR&BIT11) // PA11

#define LED1_Set0 (GPIOA->ODR &= ~BIT7)
#define LED1_Set1 (GPIOA->ODR |= BIT7)
#define LED2_Set0 (GPIOA->ODR &= ~BIT6)
#define LED2_Set1 (GPIOA->ODR |= BIT6)



// GetPeriod() seems to work fine for frequencies between 300Hz and 600kHz.
// 'n' is used to measure the time of 'n' periods; this increases accuracy.
long int GetPeriod (int n)
{
	int i;
	unsigned int saved_TCNT1a, saved_TCNT1b;
	
	SysTick->LOAD = 0xffffff;  // 24-bit counter set to check for signal present
	SysTick->VAL = 0xffffff; // load the SysTick counter
	SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; // Enable SysTick IRQ and SysTick Timer */
	while (PIN_PERIOD!=0) // Wait for square wave to be 0
	{
		if(SysTick->CTRL & BIT16) return 0;
	}
	SysTick->CTRL = 0x00; // Disable Systick counter

	SysTick->LOAD = 0xffffff;  // 24-bit counter set to check for signal present
	SysTick->VAL = 0xffffff; // load the SysTick counter
	SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; // Enable SysTick IRQ and SysTick Timer */
	while (PIN_PERIOD==0) // Wait for square wave to be 1
	{
		if(SysTick->CTRL & BIT16) return 0;
	}
	SysTick->CTRL = 0x00; // Disable Systick counter
	
	SysTick->LOAD = 0xffffff;  // 24-bit counter reset
	SysTick->VAL = 0xffffff; // load the SysTick counter to initial value
	SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; // Enable SysTick IRQ and SysTick Timer */
	for(i=0; i<n; i++) // Measure the time of 'n' periods
	{
		while (PIN_PERIOD!=0) // Wait for square wave to be 0
		{
			if(SysTick->CTRL & BIT16) return 0;
		}
		while (PIN_PERIOD==0) // Wait for square wave to be 1
		{
			if(SysTick->CTRL & BIT16) return 0;
		}
	}
	SysTick->CTRL = 0x00; // Disable Systick counter

	return 0xffffff-SysTick->VAL;
}

// LQFP32 pinout
//					   ----------
//				 VDD -|1       32|- VSS
//				PC14 -|2       31|- BOOT0
//				PC15 -|3       30|- PB7 
//				NRST -|4       29|- PB6 
//			    VDDA -|5       28|- PB5 
//				 PA0 -|6       27|- PB4
//				 PA1 -|7       26|- PB3 
//				 PA2 -|8       25|- PA15 Button 1
//				 PA3 -|9       24|- PA14 Button 2
//				 PA4 -|10      23|- PA13 Button 3
//			 	 PA5 -|11      22|- PA12 Button 4
//		LEDG	 PA6 -|12      21|- PA11 Button 5
//		LEDR	 PA7 -|13      20|- PA10 RXD
//				 PB0 -|14      19|- PA9  TXD
//				 PB1 -|15      18|- PA8  Period
//				 VSS -|16      17|- VDD
//				       ----------

void main(void)
{
	long int count;
	float T, f, C;
	char linebuffer[17];

	Configure_Pins();
	LCD_4BIT();

	RCC->IOPENR |= 0x00000001; // peripheral clock enable for port A
	
	Set_Pin_Input(8,1);
	Set_Pin_Input(15,1);
	Set_Pin_Input(14,1);
	Set_Pin_Input(13,1);
	Set_Pin_Input(12,1);
	Set_Pin_Input(11,1);
	Set_Pin_Output(0,6,0);
	Set_Pin_Output(0,7,0);

	waitms(500); // Wait for putty to start.
	printf("Period measurement using the Systick free running counter.\r\n"
	      "Connect signal to PA8 (pin 18).\r\n");
	
	while(1)
	{
		count=GetPeriod(100);
		if(count>0)
		{
			T=count/(F_CPU*100.0); // Since we have the time of 100 periods, we need to divide by 100
			f=1.0/T;
			C = 1.0/(f*factor) *10e9;
			// printf("f=%.2fHz, count=%d            \r", f, count);
			//                  1234567890123456
			sprintf(linebuffer,"C=%.2fnF",C);
			LCDprint(linebuffer,1,1);
			sprintf(linebuffer,"f=%.2fHz",f);
			LCDprint(linebuffer,2,1);

		}
		else
		{
			printf("NO SIGNAL                     \r");
			LCDprint("No signal",1,1);
		}

		if (!Button1){
		 	while(!Button1);
		 	printf("PA15(25) Pressed!\n\r");
		}
		if (!Button2){
			while(!Button2);
			printf("PA14(24) Pressed!\n\r");
	    }
	    if (!Button3){
			while(!Button3);
			printf("PA13(23) Pressed!\n\r");
   		}
		if (!Button4){
		 	while(!Button4);
		 	printf("PA12(22) Pressed!\n\r");
		}
		if (!Button5){
			while(!Button5);
			printf("PA11(21) Pressed!\n\r");
	    }

		LED1_Set1;
		LED2_Set1;
		waitms(500);
		LED1_Set0; 
		LED2_Set0;


		fflush(stdout); // GCC printf wants a \n in order to send something.  If \n is not present, we fflush(stdout)
		waitms(200);
	}
}
