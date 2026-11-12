// State shared between neonpine.c (main loop) and usb.c (the USB interrupt: rv003usb + the HID report hooks).
#pragma once
#include <stdint.h>

struct np_status {
	uint8_t  report_id;   //  0 = NP_REPORT_STATUS
	uint8_t  fw_version;  //  1
	uint8_t  mode;        //  2
	uint8_t  step;        //  3 test sequence step
	uint8_t  fault;       //  4
	uint8_t  flags;       //  5 b0 touched, b1 paused, b2 boost running
	uint8_t  hv_set;      //  6 V (current HV target)
	uint8_t  period_us;   //  7 boost period, us
	uint16_t hv_cv;       //  8 measured HV, 10 mV units
	uint16_t vdd_mv;      // 10 from Vrefint
	uint16_t adc_vfb;     // 12
	uint16_t adc_ldr;     // 14 light sensor, average with the lamps as they are
	uint16_t touch;       // 16 raw touch sum
	uint16_t touch_base;  // 18
	uint16_t ton;         // 20 gate on-time, ticks
	uint16_t duty_pm;     // 22 share of boost periods that pulsed, per mille
	uint32_t uptime_ms;   // 24
	uint32_t lamps;       // 28 which electrodes are lit (2 bits per lamp: b0 left/K, b1 right/A)
	uint8_t  level;       // 32 brightness 0-255 (test shows: level, animations: duty factor)
	uint8_t  budget;      // 33 power budget scale, 255 = no limit
	uint8_t  pwm_ms;      // 34 PWM frame
	uint8_t  pwm_min_10us;// 35 minimum on-time, 10 us units
	uint8_t  show;        // 36
	uint8_t  style;       // 37
	uint8_t  user_level;  // 38 overall brightness (touch hold ramp)
	uint8_t  gesture;     // 39 last gesture (low nibble) + event counter (high nibble)
	uint8_t  sleep_min;   // 40 sleep timer, minutes left (0 = off)
	uint8_t  flags2;      // 41 b0 sleep timer, b1 off for daylight, b2 light sensor features, b3 rail dimming
	uint16_t ambient;     // 42 light sensor in the dark slot (room light only), ADC counts
	uint16_t reflection;  // 44 own light seen by the sensor per unit duty
	uint8_t  lux;         // 46
	uint8_t  adapt;       // 47 room-adaptive brightness factor, /255
	uint16_t late_us;     // 48 latest lamp edge in the last 100 ms (how late an on/off switch happened)
	uint16_t late_peak_us;// 50 the same since the previous status read
	uint8_t  caps;        // 52 b0 store, b1 live frames, b2 animation parameters
	uint8_t  store_pages; // 53
	uint8_t  n_custom;    // 54 valid custom animations in the store
	uint8_t  cfg_flags;   // 55 settings flags (CFG_*), b7 unsaved changes
	uint16_t store_gen;   // 56 counts store writes (the app re-reads when it changes)
	uint16_t stack_free;  // 58 bytes of stack never used since power-up
	uint32_t uid;         // 60 chip unique ID (folded to 32 bits)
} __attribute__((packed, aligned(4)));
_Static_assert( sizeof( struct np_status ) == 64, "status report is 63 bytes + id (usb_config.h)" );

// neonpine.c
extern struct np_status status;          // NP_REPORT_STATUS, refreshed by the status job
extern uint8_t cmd_buf[8];               // NP_REPORT_COMMAND, cmd_pending = 1 when complete
extern volatile uint8_t cmd_pending;
extern uint8_t store_buf[68];            // NP_REPORT_STORE, both directions; store_lock = the main loop has it
extern volatile uint8_t store_req, store_lock, live_req;
extern volatile uint32_t store_req_ms;
extern volatile uint8_t status_read;     // the host read the status (resets late_peak)
extern uint32_t now_ms;
void np_outputs_off( void );             // boost gate low, every lamp switch off

// usb.c
void usb_disconnect( void );             // D- low for 20 ms: the host sees an unplug
