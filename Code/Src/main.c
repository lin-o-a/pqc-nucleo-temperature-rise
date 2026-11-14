/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32l5xx_ll_adc.h"
#include "api.h"
//#include "kem.h"
#include "RSA/bignum_core.h"
#include "RSA/bignum_mod.h"

#include "RSA/rsa_exp.h"

#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DS18B20_PORT GPIOB
#define DS18B20_PIN  GPIO_PIN_9  // Change to selected GPIO pin

// Define second DS18B20 port and pin (D8 = PF12)
#define DS18B20_PORT_2      GPIOF
#define DS18B20_PIN_2       GPIO_PIN_12
// Define a 2048-bit size in 32-bit limbs (2048 / 32 = 64 limbs)
#define RSA_LIMBS 64
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;
ADC_HandleTypeDef hadc1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_ICACHE_Init(void);
/* USER CODE BEGIN PFP */
void DWT_Init(void) {
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void delay_us(uint32_t us) {
	uint32_t startTicks = DWT->CYCCNT;
	uint32_t delayTicks = us * (SystemCoreClock / 1000000U);
	while ((DWT->CYCCNT - startTicks) < delayTicks)
		;
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// ==========================================
// 1. Internal CPU Die Temperature Reader
// ==========================================
float Read_Internal_ADC_Temp(void) {
	float vdda = 3.3f;

	ADC_ChannelConfTypeDef sConfig = { 0 };

	// 1. Read VREFINT (Channel 18)
	sConfig.Channel = ADC_CHANNEL_VREFINT;
	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
	sConfig.SingleDiff = ADC_SINGLE_ENDED;
	sConfig.OffsetNumber = ADC_OFFSET_NONE;
	sConfig.Offset = 0;

	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
		Error_Handler();

	if (HAL_ADC_Start(&hadc1) == HAL_OK) {
		if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK) {
			uint32_t vrefint_raw = HAL_ADC_GetValue(&hadc1);
			uint16_t vref_cal = *(__IO uint16_t*) (0x0BFA05AA);
			vdda = 3.0f * ((float) vref_cal / (float) vrefint_raw);
		}
		HAL_ADC_Stop(&hadc1);
	}

	// 2. Switch Channel to Temperature Sensor
	sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
		Error_Handler();

	float temperature = 0.00;

	if (HAL_ADC_Start(&hadc1) == HAL_OK) {
		for (volatile int i = 0; i < 800; i++)
			;

		if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK) {
			uint32_t raw_value = HAL_ADC_GetValue(&hadc1);

			int16_t ts_cal1 = *(__IO int16_t*) (0x0BFA05A8);
			int16_t ts_cal2 = *(__IO int16_t*) (0x0BFA05CA);

			float v_cal1 = ((float) ts_cal1 * 3000.0f) / 4095.0f;
			float v_cal2 = ((float) ts_cal2 * 3000.0f) / 4095.0f;
			float v_sense = ((float) raw_value * vdda * 1000.0f) / 4095.0f;

			temperature = ((110.0f - 30.0f) / (v_cal2 - v_cal1))
					* (v_sense - v_cal1) + 30.0f;

			printf("CPU Die Temperature: %.2f °C | VDDA: %.3f V | Raw: %lu\r\n",
					temperature, vdda, raw_value);
		}

		HAL_ADC_Stop(&hadc1);
	}

	return temperature;
}

// ==========================================
// 2. Parameterized DS18B20 1-Wire Drivers
// ==========================================

static void Set_Pin_Output(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

static void Set_Pin_Input(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

uint8_t DS18B20_Reset(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
	Set_Pin_Output(GPIOx, GPIO_Pin);
	HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_RESET);
	delay_us(480); // Reset pulse
	Set_Pin_Input(GPIOx, GPIO_Pin);
	delay_us(70);
	uint8_t presence = (HAL_GPIO_ReadPin(GPIOx, GPIO_Pin) == GPIO_PIN_RESET);
	delay_us(410);
	return presence;
}

void DS18B20_WriteBit(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, uint8_t bit) {
	Set_Pin_Output(GPIOx, GPIO_Pin);
	HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_RESET);
	if (bit) {
		delay_us(6);
		Set_Pin_Input(GPIOx, GPIO_Pin); // Release line
		delay_us(64);
	} else {
		delay_us(60);
		Set_Pin_Input(GPIOx, GPIO_Pin);
		delay_us(10);
	}
}

uint8_t DS18B20_ReadBit(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
	uint8_t bit = 0;
	Set_Pin_Output(GPIOx, GPIO_Pin);
	HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_RESET);
	delay_us(2);
	Set_Pin_Input(GPIOx, GPIO_Pin);
	delay_us(10);
	if (HAL_GPIO_ReadPin(GPIOx, GPIO_Pin) == GPIO_PIN_SET) {
		bit = 1;
	}
	delay_us(50);
	return bit;
}

void DS18B20_WriteByte(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, uint8_t byte) {
	for (uint8_t i = 0; i < 8; i++) {
		DS18B20_WriteBit(GPIOx, GPIO_Pin, byte & (1 << i));
	}
}

uint8_t DS18B20_ReadByte(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
	uint8_t byte = 0;
	for (uint8_t i = 0; i < 8; i++) {
		if (DS18B20_ReadBit(GPIOx, GPIO_Pin)) {
			byte |= (1 << i);
		}
	}
	return byte;
}

float DS18B20_ReadTemperature_Pin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin,
		const char *label) {
	if (!DS18B20_Reset(GPIOx, GPIO_Pin)) {
		return -999.0f; // Error: No device detected on this pin
	}

	DS18B20_WriteByte(GPIOx, GPIO_Pin, 0xCC); // Skip ROM
	DS18B20_WriteByte(GPIOx, GPIO_Pin, 0x44); // Start temperature conversion

	HAL_Delay(750); // Wait for conversion (750ms max at 12-bit resolution)

	if (!DS18B20_Reset(GPIOx, GPIO_Pin)) {
		return -999.0f;
	}

	DS18B20_WriteByte(GPIOx, GPIO_Pin, 0xCC); // Skip ROM
	DS18B20_WriteByte(GPIOx, GPIO_Pin, 0xBE); // Read Scratchpad

	uint8_t LSB = DS18B20_ReadByte(GPIOx, GPIO_Pin);
	uint8_t MSB = DS18B20_ReadByte(GPIOx, GPIO_Pin);

	int16_t rawTemp = (MSB << 8) | LSB;
	float temperature = (float) rawTemp / 16.0f;

	printf("%s Temperature: %.2f °C | Raw: 0x%04X\r\n", label, temperature,
			(unsigned int) rawTemp);

	return temperature;
}

// ==========================================
// 3. Pin Initializations
// ==========================================
void DS18B20_Pin1_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	__HAL_RCC_GPIOB_CLK_ENABLE();
	GPIO_InitStruct.Pin = GPIO_PIN_9; // D14
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void DS18B20_Pin2_Manual_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	__HAL_RCC_GPIOF_CLK_ENABLE();
	GPIO_InitStruct.Pin = GPIO_PIN_12; // D8 (PF12)
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
}

void DS18B20_Pin3_Manual_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* 1. Enable the clock for GPIOF */
	__HAL_RCC_GPIOF_CLK_ENABLE();

	/* 2. Configure PF13 parameters */
	GPIO_InitStruct.Pin = GPIO_PIN_13;          // D7 (PF13)
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; // Push-Pull mode for a strong digital high/low signal
	GPIO_InitStruct.Pull = GPIO_NOPULL; // No internal pull needed (driven actively)
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; // High speed for sharp, clean trigger edges

	/* 3. Initialize the pin */
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

	/* 4. Ensure it starts LOW */
	HAL_GPIO_WritePin(GPIOF, GPIO_PIN_13, GPIO_PIN_RESET);
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_ADC1_Init();
	MX_ICACHE_Init();
	/* USER CODE BEGIN 2 */
	// Enable the internal temperature sensor path on the ADC common instance
	// Powers on and connects the internal analog voltage reference and temperature sensor circuit to the ADC's common analog input path
	LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(&hadc1),
	LL_ADC_PATH_INTERNAL_TEMPSENSOR);

	// Initialize both pins at startup
	DS18B20_Pin1_Init();
	//DS18B20_Pin2_Manual_Init();
	DS18B20_Pin3_Manual_Init();

	DWT_Init();
	// Optional: Add a brief stabilization delay (at least 5µs required for temp sensor startup)
	for (volatile int i = 0; i < 1000; i++)
		;
	/* USER CODE END 2 */

	/* Initialize leds */
	BSP_LED_Init(LED_GREEN);
	BSP_LED_Init(LED_BLUE);
	BSP_LED_Init(LED_RED);

	/* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
	BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

	/* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
	BspCOMInit.BaudRate = 115200;
	BspCOMInit.WordLength = COM_WORDLENGTH_8B;
	BspCOMInit.StopBits = COM_STOPBITS_1;
	BspCOMInit.Parity = COM_PARITY_NONE;
	BspCOMInit.HwFlowCtl = COM_HWCONTROL_NONE;
	if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE) {
		Error_Handler();
	}

	uint8_t is_running = 0;
	// Check if Blue Button (B1 / PC13) is pressed (active low on Nucleo boards)
	if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {
		// Simple software debounce delay
		HAL_Delay(200);

		// Toggle state
		is_running = !is_running;

		if (is_running) {
			printf(">>> Execution RESUMED <<<\r\n");
		} else {
			printf(">>> Execution PAUSED <<<\r\n");
		}

		// Wait until button is released to prevent multi-triggering
		while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)
			;
	}
	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	//printf("RSA \r\n");
	//printf("ML-KEM Decryption: Temperature of Board Parts\r\n");
	printf("Idle \r\n");

	uint32_t last_temp_tick = 0;
	const uint32_t temp_interval_ms = 3000; // Sample temperature every 3 seconds

	while (1) {

		// If running, execute your cryptographic benchmarks and thermal reads
		if (is_running) {
			uint32_t current_tick = HAL_GetTick();
			if ((current_tick - last_temp_tick) >= temp_interval_ms) {
				last_temp_tick = current_tick;
			}
			//RSA_Decryption();

			//uint8_t pk[PQCLEAN_MLKEM512_CLEAN_CRYPTO_PUBLICKEYBYTES];
			uint8_t sk[PQCLEAN_MLKEM512_CLEAN_CRYPTO_SECRETKEYBYTES];
			uint8_t ct[PQCLEAN_MLKEM512_CLEAN_CRYPTO_CIPHERTEXTBYTES];
			//uint8_t ss_a[PQCLEAN_MLKEM512_CLEAN_CRYPTO_BYTES];
			uint8_t ss_b[PQCLEAN_MLKEM512_CLEAN_CRYPTO_BYTES];

			//PQCLEAN_MLKEM512_CLEAN_crypto_kem_dec(ss_b, ct, sk);

			//D7 = PF13
			//DS18B20_ReadTemperature_Pin(GPIOF, GPIO_PIN_13,
			//		"Substrate / Zone 1");
			//D8 = PF12 - CPU
			//DS18B20_ReadTemperature_Pin(GPIOF, GPIO_PIN_12, "CPU / Zone 2");

			//D7 = PF13
			//DS18B20_ReadTemperature_Pin(GPIOF, GPIO_PIN_13, "LDO / Zone 3");

			//D8 = PF12 - CPU
			//DS18B20_ReadTemperature_Pin(GPIOF, GPIO_PIN_12, "Micro USB / Zone 4");
			//D7 = PF13
			//DS18B20_ReadTemperature_Pin(GPIOF, GPIO_PIN_13, "LDO / Zone 5");

			//D8 = PF12 - CPU
			//DS18B20_ReadTemperature_Pin(GPIOF, GPIO_PIN_12, "LDO U5 / Zone 6");
			//D7 = PF13
			//DS18B20_ReadTemperature_Pin(GPIOF, GPIO_PIN_13, "Substrate (Left from CPU) / Zone 7");

			//D8 = PF12 - CPU
			DS18B20_ReadTemperature_Pin(GPIOF, GPIO_PIN_12, "CPU / Zone 2");
			//D7 = PF13
			DS18B20_ReadTemperature_Pin(GPIOF, GPIO_PIN_13,
					"Substrate (Left from CPU) / Zone 7");

			printf("----------------------------------------\r\n");
			printf("----------------------------------------\r\n");
		}
		//	}
	}
	/* USER CODE END WHILE */

	/* USER CODE BEGIN 3 */
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0)
			!= HAL_OK) {
		Error_Handler();
	}

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;
	RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
	RCC_OscInitStruct.PLL.PLLM = 1;
	RCC_OscInitStruct.PLL.PLLN = 55;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
	RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
	RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void) {

	/* USER CODE BEGIN ADC1_Init 0 */

	/* USER CODE END ADC1_Init 0 */

	ADC_MultiModeTypeDef multimode = { 0 };
	ADC_ChannelConfTypeDef sConfig = { 0 };

	/* USER CODE BEGIN ADC1_Init 1 */

	/* USER CODE END ADC1_Init 1 */

	/** Common config
	 */
	hadc1.Instance = ADC1;
	hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
	hadc1.Init.Resolution = ADC_RESOLUTION_12B;
	hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
	hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
	hadc1.Init.LowPowerAutoWait = DISABLE;
	hadc1.Init.ContinuousConvMode = DISABLE;
	hadc1.Init.NbrOfConversion = 1;
	hadc1.Init.DiscontinuousConvMode = DISABLE;
	hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
	hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc1.Init.DMAContinuousRequests = DISABLE;
	hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
	hadc1.Init.OversamplingMode = DISABLE;
	if (HAL_ADC_Init(&hadc1) != HAL_OK) {
		Error_Handler();
	}

	/** Configure the ADC multi-mode
	 */
	multimode.Mode = ADC_MODE_INDEPENDENT;
	if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK) {
		Error_Handler();
	}

	/** Configure Regular Channel
	 */
	sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
	sConfig.SingleDiff = ADC_SINGLE_ENDED;
	sConfig.OffsetNumber = ADC_OFFSET_NONE;
	sConfig.Offset = 0;
	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN ADC1_Init 2 */

	/* USER CODE END ADC1_Init 2 */

}

/**
 * @brief ICACHE Initialization Function
 * @param None
 * @retval None
 */
static void MX_ICACHE_Init(void) {

	/* USER CODE BEGIN ICACHE_Init 0 */

	/* USER CODE END ICACHE_Init 0 */

	/* USER CODE BEGIN ICACHE_Init 1 */

	/* USER CODE END ICACHE_Init 1 */

	/** Enable instruction cache in 1-way (direct mapped cache)
	 */
	if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_ICACHE_Enable() != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN ICACHE_Init 2 */

	/* USER CODE END ICACHE_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);

	/*Configure GPIO pin : PB9 */
	GPIO_InitStruct.Pin = GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
