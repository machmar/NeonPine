// NeonPine USB: the rv003usb driver and the HID report hooks (see usb_config.h and ../README.md "USB protocol").
//
// Built as its own object with ch32fun's stock flags and without LTO (Makefile), so the size flags used for the
// rest of the firmware (-msave-restore: extra calls in every prologue) never touch this path: the interrupt has a
// few microseconds to answer a low-speed packet, and this code is the timing that was tested on the boards.
// (A build with the size flags here did stop enumerating; -mtune=size was in it too, which alone explains that.)
// Including rv003usb.c here keeps the hooks in the same translation unit, so they are inlined into the driver's
// handlers as before.

#include "rv003usb.c"
#include <string.h>
#include "anim.h"
#include "np_usb.h"

// The driver calls each hook from one place: inline them there (as the whole-program LTO build did), which keeps
// the reply path short. The out-of-line copies are dropped by --gc-sections.
#define USB_HOOK __attribute__((always_inline)) inline

static uint8_t usb_set_report_id;
static uint8_t usb_reboot_armed;  // a valid 0xFD report arrived: reboot on its status stage

// D- low (SE0) for 20 ms: the host sees an unplug. The pin goes back to USB with usb_setup() or a reset.
void usb_disconnect( void )
{
	funPinMode( PD4, GPIO_CFGLR_OUT_10Mhz_PP );
	GPIOD->BSHR = 1u << ( 4 + 16 );
	Delay_Ms( 20 );
}

// Reboot into the USB bootloader (rv003usb's sequence), but drop off the bus first: D- low = SE0 for 20 ms,
// as at power-up. D- is pulled up for good (no pull-up switch), so without this the host never notices the
// restart. Linux resets the port after ~1 s of failing transfers; Windows doesn't, and never sees the bootloader.
static void usb_reboot( void )
{
	np_outputs_off();
	usb_disconnect();
	FLASH->BOOT_MODEKEYR = FLASH_KEY1;
	FLASH->BOOT_MODEKEYR = FLASH_KEY2;
	FLASH->STATR = 1 << 14;  // MODE = 0: boot the bootloader area
	FLASH->CTLR = CR_LOCK_Set;
	RCC->RSTSCKR |= 0x1000000;
	PFIC->SCTLR = 1u << 31;
}

USB_HOOK void usb_handle_user_in_request( struct usb_endpoint * e, uint8_t * scratchpad, int endp, uint32_t sendtok, struct rv003usb_internal * ist )
{
	// Only control transfers are used; the interrupt endpoint stays empty. On endpoint 0 this is the status
	// stage of the 0xFD report (e->custom): acknowledge it, then reboot.
	usb_send_empty( sendtok );
	if( !endp && usb_reboot_armed ) usb_reboot();
}

USB_HOOK void usb_handle_hid_get_report_start( struct usb_endpoint * e, int reqLen, uint32_t lValueLSBIndexMSB )
{
	int id = lValueLSBIndexMSB & 0xff;
	if( id == NP_REPORT_STATUS )
	{
		if( reqLen > (int)sizeof( status ) ) reqLen = sizeof( status );
		e->opaque = (uint8_t *)&status;
		e->max_len = reqLen;
		status_read = 1;
	}
	else if( id == NP_REPORT_STORE )
	{
		if( reqLen > (int)sizeof( store_buf ) ) reqLen = sizeof( store_buf );
		store_buf[0] = NP_REPORT_STORE;
		e->opaque = store_buf;
		e->max_len = reqLen;
	}
}

USB_HOOK void usb_handle_hid_set_report_start( struct usb_endpoint * e, int reqLen, uint32_t lValueLSBIndexMSB )
{
	int id = usb_set_report_id = lValueLSBIndexMSB & 0xff;
	int max = id == NP_REPORT_COMMAND ? (int)sizeof( cmd_buf ) : id == NP_REPORT_LIVE ? 21 : id == NP_REPORT_STORE ? (int)sizeof( store_buf ) : 0;
	usb_reboot_armed = 0;
	if( id == NP_REPORT_REBOOT ) { e->custom = 1; max = 7; }  // the status stage goes to usb_handle_user_in_request
	if( !max ) return;
	if( reqLen > max ) reqLen = max;
	e->max_len = reqLen;
}

USB_HOOK void usb_handle_user_data( struct usb_endpoint * e, int current_endpoint, uint8_t * data, int len, struct rv003usb_internal * ist )
{
	int id = usb_set_report_id;
	if( id == NP_REPORT_REBOOT )
	{
		const uint32_t * w = __builtin_assume_aligned( data, 4 );
		if( !current_endpoint && !e->count++ )  // the magic is in the first packet; ignore padding after it
			usb_reboot_armed = w[0] == 0xaa3412fd && ( w[1] & 0x00ffffff ) == 0x00ddccbb;
		return;
	}
	uint8_t * dst = id == NP_REPORT_COMMAND ? cmd_buf : id == NP_REPORT_LIVE ? anim_live : id == NP_REPORT_STORE ? store_buf : 0;
	if( current_endpoint || !dst ) return;
	if( dst == store_buf && store_lock ) { usb_set_report_id = 0; return; }  // the main loop has the buffer: dropped, the app retries
	int offset = e->count << 3;
	int torx = e->max_len - offset;
	if( torx > len ) torx = len;
	if( torx <= 0 ) return;
	memcpy( dst + offset, data, torx );  // the live frame goes straight into anim_live
	e->count++;
	if( offset + torx >= e->max_len )
	{
		if( id == NP_REPORT_COMMAND ) cmd_pending = 1;
		else if( id == NP_REPORT_LIVE ) live_req = 1;
		else { store_req = 1; store_req_ms = now_ms; }
	}
}
