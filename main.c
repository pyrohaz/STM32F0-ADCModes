#include <stm32f0xx_adc.h>
#include <stm32f0xx_rcc.h>
#include <stm32f0xx_gpio.h>
#include <stm32f0xx_misc.h>
#include <stm32f0xx_dma.h>

ADC_InitTypeDef A;
GPIO_InitTypeDef G;
NVIC_InitTypeDef N;
DMA_InitTypeDef D;

#define DMA_ADCMode

#ifndef DMA_ADCMode

/*********************************************************/
/*********************************************************/
/***********  Scan mode, single shot conversion **********/
/*********************************************************/
/*********************************************************/

//Variables to store current states
volatile uint8_t Converted = 0, CChan = 0;

//Variable to store current conversions
volatile uint16_t Conversions[3] = {0};

//End of conversion interrupt handler!
void ADC1_COMP_IRQHandler(void){
	//Check for end of conversion
	if(ADC_GetITStatus(ADC1, ADC_IT_EOC)){
		//Clear interrupt bit
		ADC_ClearITPendingBit(ADC1, ADC_IT_EOC);
		//Switch statement dependent on current channel. First
		//channel is initialised as zero.
		switch(CChan){
		case 0:
			Conversions[0] = ADC_GetConversionValue(ADC1);
			CChan = 1;
			break;
		case 1:
			Conversions[1] = ADC_GetConversionValue(ADC1);
			CChan = 2;
			break;
		case 2:
			Conversions[2] = ADC_GetConversionValue(ADC1);
			CChan = 0;
			Converted = 1;
			break;
		}
	}
}

int main(void)
{
	//Enable clocks
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

	//Configure PA0, PA1 and PA2 as analog inputs
	G.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
	G.GPIO_Mode = GPIO_Mode_AN;
	G.GPIO_OType = GPIO_OType_PP;
	G.GPIO_PuPd = GPIO_PuPd_NOPULL;
	G.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &G);

	//Configure ADC in 12bit mode with upward scanning
	A.ADC_ContinuousConvMode = DISABLE;
	A.ADC_DataAlign = ADC_DataAlign_Right;
	A.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	A.ADC_Resolution = ADC_Resolution_12b;
	A.ADC_ScanDirection = ADC_ScanDirection_Upward;
	ADC_Init(ADC1, &A);
	ADC_Cmd(ADC1, ENABLE);

	//Enable end of conversion interrupt
	ADC_ClearITPendingBit(ADC1, ADC_IT_EOC);
	ADC_ITConfig(ADC1, ADC_IT_EOC, ENABLE);

	//Enable ADC1 interrupt
	N.NVIC_IRQChannel = ADC1_COMP_IRQn;
	N.NVIC_IRQChannelPriority = 1;
	N.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&N);

	//Configure the channels to be converted, in this case C0, C1 and
	//C2, corresponding to PA0, PA1 and PA2 respectively
	ADC_ChannelConfig(ADC1, ADC_Channel_0, ADC_SampleTime_239_5Cycles);
	ADC_ChannelConfig(ADC1, ADC_Channel_1, ADC_SampleTime_239_5Cycles);
	ADC_ChannelConfig(ADC1, ADC_Channel_2, ADC_SampleTime_239_5Cycles);

	//Wait for the ADC to be ready!
	while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_ADRDY));

	while(1)
	{
		//Start conversion
		ADC_StartOfConversion(ADC1);

		//Wait for conversion
		while(!Converted);

		//Reset converted flag (placement of breakpoint!
		Converted = 0;
	}
}

#else

/*********************************************************/
/*********************************************************/
/***********  Scan mode, single shot conversion **********/
/*********************************************************/
/*********************************************************/

//Variable to store current conversion status
volatile uint8_t Converted = 0;

//Variable to store conversions
uint16_t Conversions[3];

//Interrupt for DMA1 Channel 1. Used to notify the end of
//conversions.
void DMA1_Channel1_IRQHandler(void){
	if(DMA_GetITStatus(DMA1_IT_TC1)){
		DMA_ClearITPendingBit(DMA1_IT_TC1);
		Converted = 1;
	}
}

int main(void)
{
	//Enable clocks
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

	//Configure PA0, PA1 and PA2 as analog inputs
	G.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
	G.GPIO_Mode = GPIO_Mode_AN;
	G.GPIO_OType = GPIO_OType_PP;
	G.GPIO_PuPd = GPIO_PuPd_NOPULL;
	G.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &G);

	//Configure ADC for DMA
	A.ADC_ContinuousConvMode = DISABLE;
	A.ADC_DataAlign = ADC_DataAlign_Right;
	A.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	A.ADC_Resolution = ADC_Resolution_12b;
	A.ADC_ScanDirection = ADC_ScanDirection_Upward;
	ADC_Init(ADC1, &A);
	ADC_Cmd(ADC1, ENABLE);
	ADC_DMACmd(ADC1, ENABLE);

	//Configure the corresponding DMA stream for the ADC
	D.DMA_BufferSize = 3;										//Three variables
	D.DMA_DIR = DMA_DIR_PeripheralSRC;							//ADC peripheral is the data source
	D.DMA_M2M = DMA_M2M_Disable;								//Disable memory to memory mode
	D.DMA_MemoryBaseAddr = (uint32_t) &Conversions[0];			//Pointer to variables array
	D.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;			//'Conversions' is 16bits large (HWord)
	D.DMA_MemoryInc = DMA_MemoryInc_Enable;						//Enable memory increment
	D.DMA_Mode = DMA_Mode_Normal;								//Non circular DMA mode
	D.DMA_PeripheralBaseAddr = (uint32_t) &ADC1->DR;			//Pointer to ADC data register!
	D.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;	//ADC1->DR is 16bits!
	D.DMA_PeripheralInc = DMA_PeripheralInc_Disable;			//Disable peripheral increment
	D.DMA_Priority = DMA_Priority_Low;							//A low priority DMA stream, not a big deal here!
	DMA_Init(DMA1_Channel1, &D);
	DMA_Cmd(DMA1_Channel1, ENABLE);

	//Enable transfer complete interrupt for DMA1 channel 1
	DMA_ClearITPendingBit(DMA1_IT_TC1);
	DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);

	N.NVIC_IRQChannel = DMA1_Channel1_IRQn;
	N.NVIC_IRQChannelPriority = 1;
	N.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&N);

	//Configure channels to be converted
	ADC_ChannelConfig(ADC1, ADC_Channel_0, ADC_SampleTime_239_5Cycles);
	ADC_ChannelConfig(ADC1, ADC_Channel_1, ADC_SampleTime_239_5Cycles);
	ADC_ChannelConfig(ADC1, ADC_Channel_2, ADC_SampleTime_239_5Cycles);

	//Wait for ADC to be ready!
	while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_ADRDY));

	while(1)
	{
		//Disable the DMA channel
		DMA_Cmd(DMA1_Channel1, DISABLE);

		//Re-initialize channel
		DMA_Init(DMA1_Channel1, &D);

		//Enable the DMA channel
		DMA_Cmd(DMA1_Channel1, ENABLE);

		//Kick off the first conversion!
		ADC_StartOfConversion(ADC1);

		//Wait for converted data
		while(!Converted);

		//Reset conversion flag (Breakpoint to read data here!)
		Converted = 0;
	}
}
#endif
