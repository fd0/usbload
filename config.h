/* configuratino file for usbload */

/* uncomment this if you need to define some other signature bytes */
//#define SIGNATURE_BYTES 0x23, 0x24, 0x25, 0

/* uncomment this if you don't want to catch the eeprom isp
 * bytewise read/write commands.  saves ~34 byte */
//#define DISABLE_CATCH_EEPROM_ISP

/* uncomment this for debug information via uart */
//#define DEBUG_UART
