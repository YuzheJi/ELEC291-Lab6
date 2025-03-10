#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "../Common/Include/stm32l051xx.h"
#include "../Common/Include/serial.h"
#include "lcd.h"


#define F_CPU 32000000L
#define factor 136752.136752


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
#define Nof (GPIOA->IDR&BIT15) // PA15
#define Setting_page (GPIOA->IDR&BIT14) // PA14
#define Mode (GPIOA->IDR&BIT13) // PA13
#define Record_page (GPIOA->IDR&BIT12) // PA12
#define Rec (GPIOA->IDR&BIT11) // PA11

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
//				 PA3 -|9       24|- PA14 Setting_page
//				 PA4 -|10      23|- PA13 Mode
//			 	 PA5 -|11      22|- PA12 Record_page
//		LEDG	 PA6 -|12      21|- PA11 Button 5 Rec
//		LEDR	 PA7 -|13      20|- PA10 RXD
//				 PB0 -|14      19|- PA9  TXD
//				 PB1 -|15      18|- PA8  Period
//				 VSS -|16      17|- VDD
//				       ----------

#define LEDR_Set0 (GPIOA->ODR &= ~BIT7)
#define LEDR_Set1 (GPIOA->ODR |= BIT7)
#define LEDG_Set0 (GPIOA->ODR &= ~BIT6)
#define LEDG_Set1 (GPIOA->ODR |= BIT6)



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

int record(int* caprec, int rec_count, float cin){
	caprec[rec_count] = cin * 1000;
	rec_count++;
	//        1234567890123456
	LCDprint("Capacitance:",1,1);
	LCDprint(" Data recorded!",2,1);
	waitms(1000);
	return rec_count;
}

int delete(int* cap1000, int rec_count, int delet_index){
	
	int i;
	//        1234567890123456
	LCDprint("Confirm delete?",1,1);
	LCDprint("YES          NO",2,1);

	while(1){
		if(!Rec){
			while(!Rec);
			if(rec_count-1 < delet_index){
				LCDprint("Delete Failed:",1,1);
				LCDprint("Empty Entry....",2,1);
				waitms(1000);
				return rec_count;
			}
		
			for(i=delet_index;i<29;i++){
				cap1000[i] = cap1000[i+1];
			}
			cap1000[30] = 0;
			rec_count--;
			return rec_count;
		}

		if(!Mode){
			while(!Mode);
			return rec_count;
		}	
	}
}

int rec_page(int* cap1000, int rec_count){
	int count = 0;
	int rec_count_func=rec_count;
	char lb[17];

	
	while(1){
		if(count < 29){
			sprintf(lb,"%2d.C=%-7.3fnF <",count+1,(float)cap1000[count]/1000);
			LCDprint(lb,1,1);
			sprintf(lb,"%2d.C=%-7.3fnF  ",count+2,(float)cap1000[count+1]/1000);
			LCDprint(lb,2,1);
		}
		else{
			sprintf(lb,"%2d.C=%-7.3fnF <",count+1,(float)cap1000[count]/1000);
			LCDprint(lb,1,1);
			LCDprint("  ",2,1);
		}
		if(!Rec){
			while(!Rec);
			if(count > 0) count --;
		}

		if(!Mode){
			while(!Mode);
			if(count < 29) count ++;
		}

		if(!Nof){
			while(!Nof);
			rec_count_func=delete(cap1000, rec_count_func, count);
		}

		if(!Record_page){
			while(!Record_page);
			return rec_count_func;
		}
	}
}

void man_set(int* error, int* cap_chk){
	char lb[17];
	int i;
	int buffer;
	int multi;
	printf("Enter the Capacitance to check(nF, int):");
	fflush(stdout); // GCC peculiarities: need to flush stdout to get string out without a '\n'
	egets_echo(lb, sizeof(lb));
	printf("\r\n");
	for(i=0; i<sizeof(lb); i++)
	{
		if(lb[i]=='\n') lb[i]=0;
		if(lb[i]=='\r') lb[i]=0;
	}
	buffer = 0;
	multi = 1;
	for(i=strlen(lb)-1; i>=0;i--){
		buffer += ((lb[i])-'0')*multi; 
		multi *= 10;
	}
	*cap_chk = buffer;

	printf("Enter the Error to check(%%, int):");
	fflush(stdout); // GCC peculiarities: need to flush stdout to get string out without a '\n'
	egets_echo(lb, sizeof(lb));
	printf("\r\n");
	for(i=0; i<sizeof(lb); i++)
	{
		if(lb[i]=='\n') lb[i]=0;
		if(lb[i]=='\r') lb[i]=0;
	}

	buffer = 0;
	multi = 1;
	for(i=strlen(lb)-1; i>=0;i--){
		buffer += ((lb[i])-'0')*multi; 
		multi *= 10;
	}
	*error = buffer;
}

void setting_page(int* error_percent, int* cap_chk, int* error_preset, int* cap_chk_preset){
	int err_count = 0;
	int cap_count = 0;
	char lb[17];

	while(1){
		sprintf(lb,"Cap_chk:  %4dnF ",*(cap_chk));
		LCDprint(lb,1,1);
		sprintf(lb,"Error:    %4d%% ",*(error_percent));
		LCDprint(lb,2,1);

		if(!Rec){
			while(!Rec);
			if(cap_count%6 == 5) cap_count = 0;
			else cap_count++;
			*cap_chk = cap_chk_preset[cap_count];
		}

		if(!Mode){
			while(!Mode);
			if(err_count%5 == 4) err_count = 0;
			else err_count++;
			*error_percent = error_preset[err_count];
		}

		if(!Setting_page){
			while(!Setting_page);
			return;
		}

		if(!Nof){
			while(!Nof);
			man_set(error_percent,cap_chk);
		}

	}
}

bool check_cap(float C, int error_percent, int cap_chk){
	int diff10;
	diff10 = 1000.0*fabsf(C-cap_chk)/cap_chk;
	if(diff10/10.0>cap_chk){
		LEDG_Set0;
		LEDR_Set1;
		printf("Capacitance:%.4fnF Err: %.4f\r",C,(float)diff10/100000.0);
		return 0;
	} 
	else{
		LEDG_Set1;
		LEDR_Set0;
		printf("Capacitance:%.4fnF Err: %.4f\r",C,(float)diff10/100000.0);
		return 1;
	}
	
}


void main(void)
{	
	bool chk_flag;
	long int count;
	float T, f, C;			// C in nF
	char linebuffer[17];
	int cap1000[30] ={0};
	int mode = 0;
	int rec_count = 0;

	int error_percent = 5;
	int cap_chk = 1;
	int error_preset[5]={10,20,30,40,45};
	int cap_chk_preset[6]={1,10,50,100,500,1000};

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

	LCDprint("ELEC 291 Lab6: ",1,1);
	waitms(300);
	LCDprint("  Capactitance ",2,1);

	waitms(800); // Wait for putty to start.
	printf("Lab 6 Capacitance with STM32\r\n");
	
	while(1)
	{
		count=GetPeriod(100);
		if(count>0)
		{
			T=count/(F_CPU*100.0); // Since we have the time of 100 periods, we need to divide by 100
			f=1.0/T;
			C = factor/(f) - 1.229;
			switch (mode){
			case 0:
					    //1234567890123456
				LCDprint("Measure:      ON",1,1);
				if(fabsf(C)<0.005){
					LCDprint("No capacitor...",2,1);
					printf("Capacitance:%.4fnF(0nF)\r",C);
					LEDG_Set0;
					LEDR_Set1;
				}
				else{
					sprintf(linebuffer,"C=%.3fnF",C);
					LCDprint(linebuffer,2,1);
					printf("Capacitance:%.4fnF\r",C);
					LEDG_Set0;
					LEDR_Set0;
				}	
				break;
			case 1:
					    //1234567890123456
				LCDprint("Measure:     CHK",1,1);
				chk_flag = check_cap(C, error_percent, cap_chk);
				if(!chk_flag) sprintf(linebuffer,"C=%.3fnF  !!!",C);
				else		  sprintf(linebuffer,"C=%.3fnF  OK!",C);
				LCDprint(linebuffer,2,1);
				break;

			case 2:
					    //1234567890123456
				LCDprint("Measure:     OFF",1,1);
				LCDprint("Press to start",2,1);
				LEDG_Set1;
				LEDR_Set1;
				break;
			
			default:
				break;
			}
		}

		if (!Nof){
		 	while(!Nof);
		}
		if (!Setting_page){
			while(!Setting_page);
			printf("Editing settings              \r");
			fflush(stdout);
			setting_page(&error_percent, &cap_chk, error_preset, cap_chk_preset);
	    }
	    if (!Mode){
			while(!Mode);
			//printf("PA13(23) Pressed!\n\r");
			if (mode==2) mode = 0;
			else mode++;
   		}
		if (!Record_page){
		 	while(!Record_page);
			fflush(stdout);
			printf("Viewing records:               \r");
			rec_count = rec_page(cap1000, rec_count);
		}
		if (!Rec){
			while(!Rec);
			rec_count = record(cap1000, rec_count, C);
	    }

		fflush(stdout); // GCC printf wants a \n in order to send something.  If \n is not present, we fflush(stdout)
		waitms(50);
	}
}
