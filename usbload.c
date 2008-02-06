/* USBasp compatible bootloader
 * (c) by Alexander Neumann <alexander@lochraster.org>
 *
 * inspired by USBasploader by Christian Starkjohann,
 * see http://www.obdev.at/products/avrusb/usbasploader.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * For more information on the GPL, please go to:
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/boot.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <string.h>

#include "config.h"
#include "usbdrv/usbdrv.c"

/* USBasp requests, taken from the original USBasp sourcecode */
#define USBASP_FUNC_CONNECT     1
#define USBASP_FUNC_DISCONNECT  2
#define USBASP_FUNC_TRANSMIT    3
#define USBASP_FUNC_READFLASH   4
#define USBASP_FUNC_ENABLEPROG  5
#define USBASP_FUNC_WRITEFLASH  6
#define USBASP_FUNC_READEEPROM  7
#define USBASP_FUNC_WRITEEEPROM 8
#define USBASP_FUNC_SETLONGADDRESS 9

/* additional functions */
#define FUNC_ECHO               0x17

/* atmel isp commands */
#define ISP_CHIP_ERASE1         0xAC
#define ISP_CHIP_ERASE2         0x80
#define ISP_READ_SIGNATURE      0x30
#define ISP_READ_EEPROM         0xa0
#define ISP_WRITE_EEPROM        0xc0

/* some predefined signatures, taken from the original USBasp sourcecode */
static const uint8_t signature[4] = {
#ifdef SIGNATURE_BYTES
    SIGNATURE_BYTES
#elif defined (__AVR_ATmega8__) || defined (__AVR_ATmega8HVA__)
    0x1e, 0x93, 0x07, 0
#elif defined (__AVR_ATmega48__) || defined (__AVR_ATmega48P__)
    0x1e, 0x92, 0x05, 0
#elif defined (__AVR_ATmega88__) || defined (__AVR_ATmega88P__)
    0x1e, 0x93, 0x0a, 0
#elif defined (__AVR_ATmega168__) || defined (__AVR_ATmega168P__)
    0x1e, 0x94, 0x06, 0
#elif defined (__AVR_ATmega328P__)
    0x1e, 0x95, 0x0f, 0
#else
#   error "Device signature is not known, please edit config.h!"
#endif
};

/* global variables */
enum {
    STATE_IDLE = 0,
    STATE_LEAVE,
} state;


/* use 16 or 32 bit counter, according to the flash page count of the target device */
#if BOOT_SECTION_START <= 65535
#   define FLASH_ADDR_T uint16_t
#else
#   define FLASH_ADDR_T uint32_t
#endif

uchar   usbFunctionSetup(uchar data[8])
{
    usbRequest_t *req = (void *)data;
    uint8_t len = 0;
    static uint8_t buf[4];

    /* start flash (byte address, converted) write at this address */
    FLASH_ADDR_T flash_address;

    /* set global data pointer to local buffer */
    usbMsgPtr = buf;

    /* on enableprog just return one zero, which means success */
    if (req->bRequest == USBASP_FUNC_ENABLEPROG) {
        buf[0] = 0;
        len = 1;

    } else if (req->bRequest == USBASP_FUNC_CONNECT) {
        /* turn on led */
        PORTB &= ~_BV(PB1);
    } else if (req->bRequest == USBASP_FUNC_DISCONNECT) {
        /* turn off led */
        PORTB |= _BV(PB1);
    /* catch query for the devicecode, chip erase and eeprom byte requests */
    } else if (req->bRequest == USBASP_FUNC_TRANSMIT) {

        /* reset buffer with zeroes */
        memset(buf, '\0', sizeof(buf));

        /* read the address for eeprom operations */
        usbWord_t address;
        address.bytes[0] = data[4]; /* low byte is data[4] */
        address.bytes[1] = data[3]; /* high byte is data[3] */

        /* if this is a request to read the device signature, answer with the
         * appropiate signature byte */
        if (data[2] == ISP_READ_SIGNATURE) {
            /* the complete isp data is reported back to avrdude, but we just need byte 4
             * bits 0 and 1 of byte 3 determine the signature byte address */
            buf[3] = signature[data[4] & 0x03];

        /* catch eeprom read */
        } else if (data[2] == ISP_READ_EEPROM) {

            buf[3] = eeprom_read_byte((uint8_t *)address.word);

        /* catch eeprom write */
        } else if (data[2] == ISP_WRITE_EEPROM) {

            /* address is in data[4], data[3], and databyte is in data[5] */
            eeprom_write_byte((uint8_t *)address.word, data[5]);

        /* catch a chip erase */
        } else if (data[2] == ISP_CHIP_ERASE1 && data[3] == ISP_CHIP_ERASE2) {
            for (flash_address = 0;
                 flash_address < BOOT_SECTION_START;
                 flash_address += SPM_PAGESIZE)
                boot_page_erase_safe(flash_address);
        }

        /* in case no data has been filled in by the if's above, just return zeroes */
        len = 4;

    } else if (req->bRequest == FUNC_ECHO) {
        buf[0] = req->wValue.bytes[0];
        buf[1] = req->wValue.bytes[1];
        len = 2;
    }

    return len;
}

uchar usbFunctionWrite(uchar *data, uchar len)
{
#if 0
uchar   isLastWrite;

    DBG1(0x31, (void *)&currentAddress.l, 4);
    if(len > bytesRemaining)
        len = bytesRemaining;
    bytesRemaining -= len;
    isLastWrite = bytesRemaining == 0;
    if(currentRequest >= USBASP_FUNC_READEEPROM){
        eeprom_write_block(data, (void *)currentAddress.w[0], len);
        currentAddress.w[0] += len;
    }else{
        char i = len;
        while(i > 0){
            i -= 2;
            if((currentAddress.w[0] & (SPM_PAGESIZE - 1)) == 0){    /* if page start: erase */
                DBG1(0x33, 0, 0);
#ifndef NO_FLASH_WRITE
                cli();
                boot_page_erase(CURRENT_ADDRESS);   /* erase page */
                sei();
                boot_spm_busy_wait();               /* wait until page is erased */
#endif
            }
            DBG1(0x32, 0, 0);
            cli();
            boot_page_fill(CURRENT_ADDRESS, *(short *)data);
            sei();
            CURRENT_ADDRESS += 2;
            data += 2;
            /* write page when we cross page boundary or we have the last partial page */
            if((currentAddress.w[0] & (SPM_PAGESIZE - 1)) == 0 || (i <= 0 && isLastWrite && isLastPage)){
                DBG1(0x34, 0, 0);
#ifndef NO_FLASH_WRITE
                cli();
                boot_page_write(CURRENT_ADDRESS - 2);
                sei();
                boot_spm_busy_wait();
                cli();
                boot_rww_enable();
                sei();
#endif
            }
        }
        DBG1(0x35, (void *)&currentAddress.l, 4);
    }
    return isLastWrite;
#endif
}

uchar usbFunctionRead(uchar *data, uchar len)
{
#if 0
    if(len > bytesRemaining)
        len = bytesRemaining;
    bytesRemaining -= len;
    if(currentRequest >= USBASP_FUNC_READEEPROM){
        eeprom_read_block(data, (void *)currentAddress.w[0], len);
    }else{
        memcpy_P(data, (PGM_VOID_P)CURRENT_ADDRESS, len);
    }
    CURRENT_ADDRESS += len;
    return len;
#endif
}

ISR(TIMER1_COMPA_vect) {
    PORTB ^= _BV(PB2);
}


int main(void) {

    /* start bootloader */

    /* init led pins */
    DDRB = _BV(PB1) | _BV(PB2);
    PORTB = _BV(PB1) | _BV(PB2);

    /* move interrupts to boot section */
    MCUCR = (1 << IVCE);
    MCUCR = (1 << IVSEL);

    /* init timer1 for blinking a led while in bootloader */
    TCCR1B = _BV(WGM12) | _BV(CS12) | _BV(CS10);
    OCR1A = F_CPU/1024;
    TIMSK1 = _BV(OCIE1A);

    /* enable interrupts */
    sei();

    /* initialize usb pins */
    usbInit();

    /* disconnect for ~500ms, so that the host re-enumerates this device */
    usbDeviceDisconnect();
    for (uint8_t i = 0; i < 31; i++)
        _delay_loop_2(0); /* 0 means 0x10000, 31*1/f*0x10000 =~ 508ms */
    usbDeviceConnect();

    while(state != STATE_LEAVE) {
        usbPoll();
    }

    /* leave bootloader */

    /* disable interrupts */
    cli();

    /* move interrupts to application section */
    MCUCR = (1 << IVCE);
    MCUCR = 0;

    /* reconfigure pins and timer */
    DDRB = 0;
    PORTB = 0;
    DDRD = 0;
    PORTD = 0;
    TCCR1B = 0;
    TCNT1 = 0;
    OCR1A = 0;
    TIMSK1 = 0;

}
