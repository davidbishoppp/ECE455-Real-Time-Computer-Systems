/*
 * GPIO_setup.c
 *
 *  Created on: Feb 8, 2022
 *      Author: David Bishop, Anmol Monga
 */

#include "../Libraries/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_gpio.c"

void Init_GPIO() {
	// Enable AHB1 Clock
	RCC_AHB1PeriphClockCmd(RCC_AHB1_Periph_GPIOB, ENABLE);

	// Struct to IN GPIOs
	GPIO_InitTypeDef GPIO_Struct;
	GPIO_Struct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_6 | GPIO_Pin_7 |GPIO_Pin_8;
	GPIO_Struct.GPIO_Mode = GPIO_Mode_IN;
	GPIO_Struct.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Struct.GPIO_OType = GPIO_OType_OD;
	GPIO_Struct.GPIO_PuPd = GPIO_PuPd_UP;

	GPIO_Init(GPIOC, GPIO_Struct);

	// Struct to init Analog GPIO
	//GPIO_InitTypeDef GPIO_Struct;
	//GPIO_Struct.GPIO_Pin = GPIO_Pin_3;
	//GPIO_Struct.GPIO_Mode = GPIO_Mode_AN;
}

