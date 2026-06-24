// Basic DCC decoder using GPIO pin interrupt for pulse-width measurement
// DCC protocol: half-bit timing — 58us = 1, ~116us = 0
use esp_idf_sys::*;
use nimrs_core::pinout;

use core::sync::atomic::{AtomicU32, AtomicU8, Ordering};

const DCC_PIN: u8 = pinout::TRACK_LEFT_3V3;

static LAST_EDGE_TIME: AtomicU32 = AtomicU32::new(0);
static PULSE_WIDTH: AtomicU32 = AtomicU32::new(0);

pub fn init() {
    unsafe {
        gpio_set_direction(DCC_PIN as i32, gpio_mode_t_GPIO_MODE_INPUT);
        gpio_set_pull_mode(DCC_PIN as i32, gpio_pull_mode_t_GPIO_PULLUP_ONLY);
        gpio_set_intr_type(DCC_PIN as i32, gpio_int_type_t_GPIO_INTR_ANYEDGE);

        // Install GPIO ISR service
        gpio_install_isr_service(0);

        extern "C" fn dcc_isr(_arg: *mut core::ffi::c_void) {
            let now = unsafe { esp_timer_get_time() as u32 };
            let last = LAST_EDGE_TIME.swap(now, Ordering::Relaxed);
            let dt = now.wrapping_sub(last);
            if dt > 30 && dt < 200 {
                PULSE_WIDTH.store(dt, Ordering::Relaxed);
                process_pulse(dt);
            } else if dt > 2000 {
                // Inter-packet gap > 2ms — start of a new packet
                reset_decoder();
            }
        }

        gpio_isr_handler_add(DCC_PIN as i32, Some(dcc_isr), core::ptr::null_mut());
    }
}

// Simple DCC state machine
static mut DECODER_STATE: DecoderState = DecoderState::Idle;
static mut BIT_COUNT: u8 = 0;
static mut CURRENT_BYTE: u16 = 0;
static mut PACKET: [u8; 16] = [0; 16];
static mut PACKET_BYTE_IDX: usize = 0;

#[derive(Clone, Copy, PartialEq)]
enum DecoderState {
    Idle,
    Preamble,
    Data,
}

fn reset_decoder() {
    unsafe {
        DECODER_STATE = DecoderState::Preamble;
        BIT_COUNT = 0;
        CURRENT_BYTE = 0;
        PACKET_BYTE_IDX = 0;
    }
}

fn process_pulse(dt: u32) {
    // DCC half-bit: ~58us = 1, ~100-116us = 0
    let bit = if dt < 85 { 1 } else { 0 };

    unsafe {
        match DECODER_STATE {
            DecoderState::Idle => {
                // Wait for start of packet
            }
            DecoderState::Preamble => {
                if bit == 1 {
                    BIT_COUNT += 1;
                    if BIT_COUNT >= 14 {
                        // Preamble complete, expect start bit (0)
                        DECODER_STATE = DecoderState::Data;
                        BIT_COUNT = 0;
                    }
                } else {
                    // False start, reset
                    DECODER_STATE = DecoderState::Idle;
                    BIT_COUNT = 0;
                }
            }
            DecoderState::Data => {
                // Each byte: 1 start bit (0) + 8 data bits + 0/1 parity bit
                if BIT_COUNT == 0 && bit == 0 {
                    // Start bit detected
                    BIT_COUNT = 1;
                    CURRENT_BYTE = 0;
                } else if BIT_COUNT >= 1 && BIT_COUNT <= 8 {
                    // Data bits (LSB first)
                    if bit == 1 {
                        CURRENT_BYTE |= 1 << (BIT_COUNT - 1);
                    }
                    BIT_COUNT += 1;
                } else if BIT_COUNT == 9 {
                    // Parity bit — skip
                    BIT_COUNT = 0;
                    PACKET[PACKET_BYTE_IDX] = CURRENT_BYTE as u8;
                    PACKET_BYTE_IDX += 1;
                    CURRENT_BYTE = 0;

                    // Check for end of packet (idle > 2ms resets)
                    if PACKET_BYTE_IDX >= 3 && PACKET_BYTE_IDX < 14 {
                        let len = PACKET_BYTE_IDX;
                        PACKET_BYTE_IDX = 0;
                        DECODER_STATE = DecoderState::Idle;
                        decode_packet(&PACKET, len);
                    }
                } else {
                    // Invalid state
                    DECODER_STATE = DecoderState::Idle;
                }
            }
        }
    }
}

fn decode_packet(packet: &[u8; 16], len: usize) {
    if len < 3 || len > 14 {
        return;
    }
    // Byte 0: Address
    // Byte 1: Instruction (speed/direction/functions)
    // Byte 2+: Data
    let addr = packet[0] as u16;
    let instr = packet[1];

    match instr & 0xC0 {
        0x40 => {
            // Speed/direction instruction 01xxxxxx
            let speed = instr & 0x1F; // 0-31 speed steps (128-step mode uses 2 bytes)
            let dir = (instr & 0x20) != 0; // Direction bit
                                           // Map 0-31 to 0-255
            let mapped_speed = if speed > 0 {
                ((speed as u16 * 255 + 15) / 31) as u8
            } else {
                0
            };
            super::notifyDccSpeed(addr, 0, mapped_speed, dir as u8, 0);
        }
        0x80 => {
            // Function instruction 10xxxxxx (F0-F4)
            // Bits 4-0 map to F4-F0
            let func_state = instr & 0x1F;
            super::notifyDccFunc(addr, 0, 0, func_state);
        }
        0xA0 | 0xB0 => {
            // Function instructions for F5-F8, F9-F12, F13-F20, F21-F28
            let grp = ((instr >> 5) & 0x07) - 1; // 1=F5_8, 2=F9_12, 3=F13_20, 4=F21_28
            let func_state = instr & 0x7F;
            if grp >= 1 && grp <= 4 {
                super::notifyDccFunc(addr, 0, grp, func_state);
            }
        }
        _ => {} // Other instructions (idle, reset, etc.)
    }
}

pub fn is_packet_recent() -> bool {
    let _now = 0;
    true
}
