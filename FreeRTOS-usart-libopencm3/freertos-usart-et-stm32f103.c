/* FreeRTOS-libopencm3 test to echo characters on USART1 and blink a LED

The first LED is blinked regularly. Characters from the source on USART1 are
echoed. The second LED is toggled with the reception of a character, and the
third is toggled on the transmission of a character.

The board used is the ET-STM32F103 with LEDs on port B pins 8-15,
but the test should work on the majority of STM32 based boards.
*/

/*
    FreeRTOS V8.2.3 - Copyright (C) 2011 Real Time Engineers Ltd.
*/

/* Libopencm3 includes. */
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#include "buffer.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define BUFFER_SIZE 64

/* Globals */
uint8_t send_buffer[BUFFER_SIZE+3];
uint8_t receive_buffer[BUFFER_SIZE+3];

/* for FreeRTOS */
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
extern void vPortSVCHandler( void );

/* Prototypes */
static void clock_setup(void);
static void systickSetup();
static void usart_setup(void);
static void gpio_setup(void);

/* Task priorities. */
#define mainBLINK_TASK_PRIORITY				( tskIDLE_PRIORITY + 0 )

/* The rate at which the blink task toggles the LED. */
#define mainBLINK_DELAY						( ( portTickType ) 200 / portTICK_RATE_MS )

/* The number of nano seconds between each processor clock. */
#define mainNS_PER_CLOCK ( ( unsigned portLONG ) ( ( 1.0 / ( double ) configCPU_CLOCK_HZ ) * 1000000000.0 ) )

/*-----------------------------------------------------------*/

/*
 * Configure the clocks, GPIO and other peripherals as required by the demo.
 */
static void prvSetupHardware( void );

/*
 * Simple task to echo USART characters.
 */
static void prvUsartTask( void *pvParameters );
static void prvBlinkTask( void *pvParameters );

/*-----------------------------------------------------------*/

int main( void )
{
#ifdef DEBUG
	debug();
#endif

	prvSetupHardware();

	gpio_toggle(GPIOB, GPIO9);	/* LED on/off */
	/* Start the blink task. */
	xTaskCreate( prvBlinkTask, ( signed portCHAR * ) "Flash", \
                 configMINIMAL_STACK_SIZE, NULL, mainBLINK_TASK_PRIORITY, NULL );
	gpio_toggle(GPIOB, GPIO10);	/* LED on/off */

	/* Start the usart task. */
	xTaskCreate( prvUsartTask, ( signed portCHAR * ) "USART", \
                 configMINIMAL_STACK_SIZE, NULL, mainBLINK_TASK_PRIORITY, NULL );

	/* Start the scheduler. */
	vTaskStartScheduler();
	
	/* Will only get here if there was not enough heap space to create the
	idle task. */
	return -1;
}
/*-----------------------------------------------------------*/

static void prvBlinkTask( void *pvParameters )
{
portTickType xLastExecutionTime;

	/* Initialise the xLastExecutionTime variable on task entry. */
	xLastExecutionTime = xTaskGetTickCount();

    for( ;; )
	{
		/* Simply toggle the LED periodically.  This just provides some timing
		verification. */
		vTaskDelayUntil( &xLastExecutionTime, mainBLINK_DELAY );
		gpio_toggle(GPIOB, GPIO8);	/* LED on/off */
	}
}
/*-----------------------------------------------------------*/

static void prvUsartTask( void *pvParameters )
{
    for( ;; )
	{
		uint16_t data = buffer_get(receive_buffer);
		if (data == BUFFER_EMPTY) taskYIELD();
		else
		{
			buffer_put(send_buffer, data);
			usart_enable_tx_interrupt(USART1);
		}
	}
}

/*-----------------------------------------------------------*/

static void prvSetupHardware( void )
{
	clock_setup();
    systickSetup();
	gpio_setup();
	usart_setup();
/* Setup Rx/Tx buffers for USART */
	buffer_init(send_buffer,BUFFER_SIZE);
	buffer_init(receive_buffer,BUFFER_SIZE);

}

/*-----------------------------------------------------------*/
/* USART 1 is configured for 115200 baud, no flow control and interrupt */
static void usart_setup(void)
{
	/* Enable the USART1 interrupt. */
	nvic_enable_irq(NVIC_USART1_IRQ);
	/* Setup GPIO pin GPIO_USART1_RE_TX on GPIO port A for transmit. */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART1_TX);
	/* Setup GPIO pin GPIO_USART1_RE_RX on GPIO port A for receive. */
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
		      GPIO_CNF_INPUT_FLOAT, GPIO_USART1_RX);
	/* Setup UART parameters. */
	usart_set_baudrate(USART1, 115200);
	usart_set_databits(USART1, 8);
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
	usart_set_mode(USART1, USART_MODE_TX_RX);
	/* Enable USART1 receive interrupts. */
	usart_enable_rx_interrupt(USART1);
	usart_disable_tx_interrupt(USART1);
	/* Finally enable the USART. */
	usart_enable(USART1);
}

/*-----------------------------------------------------------*/
/* GPIO Port B bits 8-15 setup for LED indicator outputs */
static void gpio_setup(void)
{
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL,
			  GPIO8 | GPIO9 | GPIO10 | GPIO11 | GPIO12 | GPIO13 | GPIO14 | GPIO15);
}

/*-----------------------------------------------------------*/
/* The processor system clock is established and the necessary peripheral

clocks are turned on */
static void clock_setup(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	/* Enable GPIOB clock (for LED GPIOs). */
	rcc_periph_clock_enable(RCC_GPIOB);

	/* Enable clocks for GPIOA clock (for GPIO_USART1_TX) and USART1. */
	rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_USART1);

}


/*--------------------------------------------------------------------------*/
/** @brief Systick Setup

Setup SysTick Timer for 1 millisecond interrupts, also enables Systick and
Systick-Interrupt
*/

static void systickSetup()
{
	/* 72MHz / 8 => 9,000,000 counts per second */
	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);

	/* 9000000/9000 = 1000 overflows per second - every 1ms one interrupt */
	/* SysTick interrupt every N clock pulses: set reload to N-1 */
	systick_set_reload(8999);

	systick_interrupt_enable();

	/* Start counting. */
	systick_counter_enable();
}

/*-----------------------------------------------------------*/
/* USART ISR */
void usart1_isr(void)
{
/* Find out what interrupted and get or send data as appropriate */
	/* Check if we were called because of RXNE. */
	if (usart_get_flag(USART1,USART_SR_RXNE))
	{
		/* If buffer full we'll just drop it */
		buffer_put(receive_buffer, (uint8_t) usart_recv(USART1));
	}
	/* Check if we were called because of TXE. */
	if (usart_get_flag(USART1,USART_SR_TXE))
	{
		/* If buffer empty, disable the tx interrupt */
		uint16_t data = buffer_get(send_buffer);
		if (data == BUFFER_EMPTY)
		{
			usart_disable_tx_interrupt(USART1);
		}
		else
		{
			usart_send(USART1, data);
		}
	}
}

/*-----------------------------------------------------------*/
/*----       ISR Overrides in libopencm3     ----------------*/
/*-----------------------------------------------------------*/

void sv_call_handler(void)
{
  	vPortSVCHandler();
}

/*-----------------------------------------------------------*/

void pend_sv_handler(void)
{
  	xPortPendSVHandler();
}

/*-----------------------------------------------------------*/

void sys_tick_handler(void)
{
  	xPortSysTickHandler();
}


