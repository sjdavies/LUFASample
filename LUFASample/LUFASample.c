/*
 * LUFASample.c
 *
 * Created: 01/09/2015 18:40:39
 *  Author: sjdavies
 */ 


#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/power.h>

#include "Descriptors.h"

#include <LUFA/Drivers/USB/USB.h>

void SetupHardware(void);
void EVENT_USB_Device_ConfigurationChanged(void);
void EVENT_USB_Device_ControlRequest(void);

/** 
 * LUFA CDC Class driver interface configuration and state information. This structure is
 * passed to all CDC Class driver functions, so that multiple instances of the same class
 * within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
{
	.Config =
		{
			.ControlInterfaceNumber         = INTERFACE_ID_CDC_CCI,
			.DataINEndpoint                 =
				{
					.Address                = CDC_TX_EPADDR,
					.Size                   = CDC_TXRX_EPSIZE,
					.Banks                  = 1,
				},
			.DataOUTEndpoint                =
				{
					.Address                = CDC_RX_EPADDR,
					.Size                   = CDC_TXRX_EPSIZE,
					.Banks                  = 1,
				},
			.NotificationEndpoint           =
				{
					.Address                = CDC_NOTIFICATION_EPADDR,
					.Size                   = CDC_NOTIFICATION_EPSIZE,
					.Banks                  = 1,
				},
		},
};

uint8_t nextChar = 0x20;
volatile uint8_t timeout = 0;

int main(void)
{
	SetupHardware();

	GlobalInterruptEnable();

    while(1)
    {
		if (timeout)
		{
			Endpoint_SelectEndpoint(VirtualSerial_CDC_Interface.Config.DataINEndpoint.Address);

			/* 
			 * Check if a packet is already enqueued to the host - if so, we shouldn't try to send more data
			 * until it completes as there is a chance nothing is listening and a lengthy timeout could occur.
			 */
			if (Endpoint_IsINReady())
			{
				/*
				 * Never send more than one bank size less one byte to the host at a time, so that we don't block
				 * while a Zero Length Packet (ZLP) to terminate the transfer is sent if the host isn't listening.
				 */
				uint8_t BytesToSend = (CDC_TXRX_EPSIZE - 1);

				/* Read bytes from the USART receive buffer into the USB IN endpoint */
				while (BytesToSend--)
				{
					/* Try to send the next byte of data to the host, abort if there is an error without dequeuing */
					if (CDC_Device_SendByte(&VirtualSerial_CDC_Interface, nextChar) != ENDPOINT_READYWAIT_NoError)
					{
						break;
					}

					nextChar++;
					if (nextChar >= 0x80)
						nextChar = 0x20;
				}
			}
			
			timeout = 0;
		}
		
		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		//USB_USBTask();
    }
}


/**
 * Configures the board hardware and chip peripherals for the demo's functionality.
 */
void SetupHardware(void)
{
#if (ARCH == ARCH_AVR8)
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);
#endif

	TCCR1A = 0;								// no outputs used, mode = Normal
	TCCR1B = (1 << CS12) | (1 << CS10);		// mode = Normal, prescalar = f/1024
	TCCR1C = 0;								// use defaults
	TIMSK1 |= (1 << TOIE1);					// Enable interrupt on T1 overflow
	
	/* Hardware Initialization */
	USB_Init();
}

ISR(TIMER1_OVF_vect)
{
	timeout = 1;
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

