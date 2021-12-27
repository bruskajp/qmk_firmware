/* Copyright 2016 Jack Humbert
 * Copyright 2019 Drashna Jael're (@drashna, aka Christopher Courtney)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Author: Wojciech Siewierski < wojciech dot siewierski at onet dot pl > */
#include "process_dynamic_macro.h"
#include <string.h>

// default feedback method
void dynamic_macro_led_blink(void) {
#ifdef BACKLIGHT_ENABLE
    backlight_toggle();
    wait_ms(100);
    backlight_toggle();
#endif
}

// TODO: JPB: These should be external defines
#define ERASE_NON_MACRO_INPUT_ON_REC_STOP

/* Internal variables for Input Macros */
char *input_macro_output = NULL;
size_t input_macro_output_len;
size_t dyn_macro_len = 0;

// TODO: JPB: Should this be a macro to make sure it is inlined?
/**
 * Determine whether the input macro is active or not
 */
inline bool input_macro_active(void) {
  return input_macro_output != NULL;
}

/**
 * Process each keycode of the input macro output
 * replacing the INPUT_MACRO_PLAY keycodes with the full dynamic macro ouput
 */
void process_input_macro(void) {
    if (input_macro_output != NULL) {
#ifdef ERASE_NON_MACRO_INPUT_ON_REC_STOP
        for (size_t i=0; i<dyn_macro_len; ++i) {
            SEND_STRING(SS_TAP(X_BSPACE));
        }
#endif
        keyrecord_t record = {{{}, false, timer_read()}, {}};
        char single_output[2] = {0,0};
        for (size_t i = 0; i < input_macro_output_len; ++i) {
            if (input_macro_output[i] == INPUT_MACRO_PLAY[0] && i != input_macro_output_len - 1) {
                  process_dynamic_macro(DYN_MACRO_PLAY1, &record);
            } else {
                single_output[0] = input_macro_output[i];
                SEND_STRING(single_output);
            }
        }

        free(input_macro_output);
        input_macro_output = NULL;
    }
}

//// TODO: JPB: Use PSTR
//#define SENG_STRING_INPUT_MACRO(string) process_input_macro(string)
//
///**
// * Process each keycode of the input macro output
// * replacing the INPUT_MACRO_PLAY keycodes with the full dynamic macro ouput
// */
//void process_input_macro(const char *str, ) {
//    if (str != NULL) {
//#ifdef ERASE_NON_MACRO_INPUT_ON_REC_STOP
//        for (size_t i=0; i<dyn_macro_len; ++i) {
//            SEND_STRING(SS_TAP(X_BSPACE));
//        }
//#endif
//        keyrecord_t record = {{{}, false, 0}, {}};
//        char single_output[2] = {0,0};
//        for (size_t i = 0; i < input_macro_output_len; ++i) {
//            if (input_macro_output[i] == INPUT_MACRO_PLAY[0]) {
//                record.event.time = timer_read();
//                process_dynamic_macro(DYN_MACRO_PLAY1, &record);
//            } else {
//                single_output[0] = input_macro_output[i];
//                SEND_STRING(single_output);
//            }
//        }
//
//        free(input_macro_output);
//        input_macro_output = NULL;
//    }
//}

/* User hooks for Dynamic Macros */
__attribute__((weak)) void dynamic_macro_record_start_user(void) { dynamic_macro_led_blink(); }

__attribute__((weak)) void dynamic_macro_play_user(int8_t direction) { dynamic_macro_led_blink(); }

__attribute__((weak)) void dynamic_macro_record_key_user(int8_t direction, keyrecord_t *record) { dynamic_macro_led_blink(); }

__attribute__((weak)) void dynamic_macro_record_end_user(int8_t direction) { dynamic_macro_led_blink(); }

/* Convenience macros used for retrieving the debug info. All of them
 * need a `direction` variable accessible at the call site.
 */
#define DYNAMIC_MACRO_CURRENT_SLOT() (direction > 0 ? 1 : 2)
#define DYNAMIC_MACRO_CURRENT_LENGTH(BEGIN, POINTER) ((int)(direction * ((POINTER) - (BEGIN))))
#define DYNAMIC_MACRO_CURRENT_CAPACITY(BEGIN, END2) ((int)(direction * ((END2) - (BEGIN)) + 1))

/**
 * Start recording of the dynamic macro.
 *
 * @param[out] macro_pointer The new macro buffer iterator.
 * @param[in]  macro_buffer  The macro buffer used to initialize macro_pointer.
 */
void dynamic_macro_record_start(keyrecord_t **macro_pointer, keyrecord_t *macro_buffer) {
    dprintln("dynamic macro recording: started");

    dynamic_macro_record_start_user();

    clear_keyboard();
    layer_clear();
    *macro_pointer = macro_buffer;
    dyn_macro_len = 0;
}

/**
 * Play the dynamic macro.
 *
 * @param macro_buffer[in] The beginning of the macro buffer being played.
 * @param macro_end[in]    The element after the last macro buffer element.
 * @param direction[in]    Either +1 or -1, which way to iterate the buffer.
 */
void dynamic_macro_play(keyrecord_t *macro_buffer, keyrecord_t *macro_end, int8_t direction) {
    dprintf("dynamic macro: slot %d playback\n", DYNAMIC_MACRO_CURRENT_SLOT());

    layer_state_t saved_layer_state = layer_state;

    clear_keyboard();
    layer_clear();

    while (macro_buffer != macro_end) {
        process_record(macro_buffer);
        macro_buffer += direction;
    }

    clear_keyboard();

    layer_state = saved_layer_state;

    dynamic_macro_play_user(direction);
}

/**
 * Record a single key in a dynamic macro.
 *
 * @param macro_buffer[in] The start of the used macro buffer.
 * @param macro_pointer[in,out] The current buffer position.
 * @param macro2_end[in] The end of the other macro.
 * @param direction[in]  Either +1 or -1, which way to iterate the buffer.
 * @param record[in]     The current keypress.
 */
void dynamic_macro_record_key(keyrecord_t *macro_buffer, keyrecord_t **macro_pointer, keyrecord_t *macro2_end, int8_t direction, keyrecord_t *record) {
    /* If we've just started recording, ignore all the key releases. */
    if (!record->event.pressed && *macro_pointer == macro_buffer) {
        dprintln("dynamic macro: ignoring a leading key-up event");
        return;
    }

    /* The other end of the other macro is the last buffer element it
     * is safe to use before overwriting the other macro.
     */
    if (*macro_pointer - direction != macro2_end) {
        **macro_pointer = *record;
        *macro_pointer += direction;
        ++dyn_macro_len;
    } else {
        dynamic_macro_record_key_user(direction, record);
    }

    dprintf("dynamic macro: slot %d length: %d/%d\n", DYNAMIC_MACRO_CURRENT_SLOT(), DYNAMIC_MACRO_CURRENT_LENGTH(macro_buffer, *macro_pointer), DYNAMIC_MACRO_CURRENT_CAPACITY(macro_buffer, macro2_end));
}

/**
 * End recording of the dynamic macro. Essentially just update the
 * pointer to the end of the macro.
 */
void dynamic_macro_record_end(keyrecord_t *macro_buffer, keyrecord_t *macro_pointer, int8_t direction, keyrecord_t **macro_end) {
    if (!input_macro_active()) {
        dynamic_macro_record_end_user(direction);
    }

    /* Do not save the keys being held when stopping the recording,
     * i.e. the keys used to access the layer DYN_REC_STOP is on.
     */
    while (macro_pointer != macro_buffer && (macro_pointer - direction)->event.pressed) {
        dprintln("dynamic macro: trimming a trailing key-down event");
        macro_pointer -= direction;
    }

    dprintf("dynamic macro: slot %d saved, length: %d\n", DYNAMIC_MACRO_CURRENT_SLOT(), DYNAMIC_MACRO_CURRENT_LENGTH(macro_buffer, macro_pointer));

    *macro_end = macro_pointer;
    
    if (input_macro_active()) {
        process_input_macro();
    }
}

/* Handle the key events related to the dynamic macros. Should be
 * called from process_record_user() like this:
 *
 *   bool process_record_user(uint16_t keycode, keyrecord_t *record) {
 *       if (!process_record_dynamic_macro(keycode, record)) {
 *           return false;
 *       }
 *       <...THE REST OF THE FUNCTION...>
 *   }
 */
bool process_dynamic_macro(uint16_t keycode, keyrecord_t *record) {
    /* Both macros use the same buffer but read/write on different
     * ends of it.
     *
     * Macro1 is written left-to-right starting from the beginning of
     * the buffer.
     *
     * Macro2 is written right-to-left starting from the end of the
     * buffer.
     *
     * &macro_buffer   macro_end
     *  v                   v
     * +------------------------------------------------------------+
     * |>>>>>> MACRO1 >>>>>>      <<<<<<<<<<<<< MACRO2 <<<<<<<<<<<<<|
     * +------------------------------------------------------------+
     *                           ^                                 ^
     *                         r_macro_end                  r_macro_buffer
     *
     * During the recording when one macro encounters the end of the
     * other macro, the recording is stopped. Apart from this, there
     * are no arbitrary limits for the macros' length in relation to
     * each other: for example one can either have two medium sized
     * macros or one long macro and one short macro. Or even one empty
     * and one using the whole buffer.
     */
    static keyrecord_t macro_buffer[DYNAMIC_MACRO_SIZE];

    /* Pointer to the first buffer element after the first macro.
     * Initially points to the very beginning of the buffer since the
     * macro is empty. */
    static keyrecord_t *macro_end = macro_buffer;

    /* The other end of the macro buffer. Serves as the beginning of
     * the second macro. */
    static keyrecord_t *const r_macro_buffer = macro_buffer + DYNAMIC_MACRO_SIZE - 1;

    /* Like macro_end but for the second macro. */
    static keyrecord_t *r_macro_end = r_macro_buffer;

    /* A persistent pointer to the current macro position (iterator)
     * used during the recording. */
    static keyrecord_t *macro_pointer = NULL;

    /* 0   - no macro is being recorded right now
     * 1,2 - either macro 1 or 2 is being recorded */
    static uint8_t macro_id = 0;

    if (macro_id == 0) {
        /* No macro recording in progress. */
        if (!record->event.pressed) {
            switch (keycode) {
                case DYN_REC_START1:
                    dynamic_macro_record_start(&macro_pointer, macro_buffer);
                    macro_id = 1;
                    return false;
                case DYN_REC_START2:
                    dynamic_macro_record_start(&macro_pointer, r_macro_buffer);
                    macro_id = 2;
                    return false;
                case DYN_MACRO_PLAY1:
                    dynamic_macro_play(macro_buffer, macro_end, +1);
                    return false;
                case DYN_MACRO_PLAY2:
                    dynamic_macro_play(r_macro_buffer, r_macro_end, -1);
                    return false;
            }
        }
    } else {
        /* A macro is being recorded right now. */
        switch (keycode) {
            case DYN_REC_START1:
            case DYN_REC_START2:
            case DYN_REC_STOP:
                /* Stop the macro recording. */
                if (record->event.pressed ^ (keycode != DYN_REC_STOP)) { /* Ignore the initial release
                                                                          * just after the recording
                                                                          * starts for DYN_REC_STOP. */
                    uint8_t old_macro_id = macro_id;
                    macro_id = 0;
                    switch (old_macro_id) {
                        case 1:
                            dynamic_macro_record_end(macro_buffer, macro_pointer, +1, &macro_end);
                            break;
                        case 2:
                            dynamic_macro_record_end(r_macro_buffer, macro_pointer, -1, &r_macro_end);
                            break;
                    }
                }
                return false;
#ifdef DYNAMIC_MACRO_NO_NESTING
            case DYN_MACRO_PLAY1:
            case DYN_MACRO_PLAY2:
                dprintln("dynamic macro: ignoring macro play key while recording");
                return false;
#endif
            default:
                /* Store the key in the macro buffer and process it normally. */
                switch (macro_id) {
                    case 1:
                        dynamic_macro_record_key(macro_buffer, &macro_pointer, r_macro_end, +1, record);
                        break;
                    case 2:
                        dynamic_macro_record_key(r_macro_buffer, &macro_pointer, macro_end, -1, record);
                        break;
                }
                return true;
        }
    }

    return true;
}

// TODO: JPB: Should I just make this use the value already stores in the macros
// TODO: JPB: Can I avoid malloc somehow?
//            Can I just make it a static variable?
//            Should I just assign a large buffer?
//            Should it only expand as needed?
void input_macro_start(char *output, size_t output_len, keyrecord_t *record) {
  if (output != NULL) { 
      input_macro_output = malloc(output_len * sizeof(output[0]));
      memcpy(input_macro_output, output, output_len * sizeof(output[0]));
      input_macro_output_len = output_len;
      process_dynamic_macro(DYN_REC_START1, record);
  }
}
