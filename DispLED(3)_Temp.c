#include "DispLED(2)_Temp.h"

/* --- CONFIG: ADJUST THESE FOR YOUR HARDWARE --- */
#define SEGMENT_ACTIVE_HIGH    0    // 0 = segments active LOW (common anode), 1 = segments active HIGH (common cathode)
#define DIGIT_ACTIVE_HIGH      1    // 0 = digits active LOW, 1 = digits active HIGH
#define NUM_DIGITS             4
#define SPI_BYTE_ORDER         0    // 0 = Segment byte first, 1 = Digit byte first (TRY BOTH!)

/* --- internal state --- */
static SPI_HandleTypeDef *hspi_ptr = NULL;
static GPIO_TypeDef *latch_port = NULL;
static uint16_t latch_pin = 0;
static uint8_t digits_buf[NUM_DIGITS] = {SEG_BLANK, SEG_BLANK, SEG_BLANK, SEG_BLANK};
static uint8_t current_digit = 0;
static uint32_t last_refresh_time = 0;
static uint16_t refresh_interval_ms = 2;

/*
   7-Segment Font for Common Cathode (segments active HIGH)
   Bit mapping:
      bit0 = A, bit1 = B, bit2 = C, bit3 = D,
      bit4 = E, bit5 = F, bit6 = G, bit7 = DP

   Segment layout:
        A
       ---
    F |   | B
       -G-
    E |   | C
       ---
        D
*/
static const uint8_t seg_font[] = {
    /* 0-9 */
    0x3F, /* 0: ABCDEF */
    0x06, /* 1: BC */
    0x5B, /* 2: ABDEG */
    0x4F, /* 3: ABCDG */
    0x66, /* 4: BCFG */
    0x6D, /* 5: ACDFG */
    0x7D, /* 6: ACDEFG */
    0x07, /* 7: ABC */
    0x7F, /* 8: ABCDEFG */
    0x6F, /* 9: ABCDFG */

    /* A-F */
    0x77, /* A: ABEFG */
    0x7C, /* b: CDEFG */
    0x39, /* C: ADEF */
    0x5E, /* d: BCDEG */
    0x79, /* E: ADEFG */
    0x71, /* F: AEFG */

    /* Special */
    0x00, /* Blank */
    0x40, /* Minus (-) */

    /* Letters */
    0x3D, /* G */
    0x76, /* H */
    0x30, /* I */
    0x1E, /* J */
    0x38, /* L */
    0x54, /* N */
    0x3F, /* O */
    0x73, /* P */
    0x67, /* Q */
    0x50, /* R */
    0x6D, /* S */
    0x78, /* T */
    0x3E, /* U */
    0x6E, /* Y */
    0x5B, /* Z */
    0x63  /* ° */
};

/* Helper function to set decimal point */
static uint8_t set_decimal_point(uint8_t digit) {
    return digit | 0x80;  // Set bit 7
}

/* Helper function to get segment pattern */
static uint8_t get_segments(uint8_t digit_code) {
    // Check if decimal point is set
    uint8_t has_dp = digit_code & 0x80;
    uint8_t clean_code = digit_code & 0x7F;

    // Get segment pattern
    uint8_t segments;
    if (clean_code < sizeof(seg_font)) {
        segments = seg_font[clean_code];
    } else {
        segments = seg_font[SEG_BLANK];
    }

    // Add decimal point if needed
    if (has_dp) {
        segments |= 0x80;
    }

    // Invert if segments are active low
    if (!SEGMENT_ACTIVE_HIGH) {
        segments = ~segments;
    }

    return segments;
}

/* Initialize display */
void DISP_Init(SPI_HandleTypeDef *hspi, GPIO_TypeDef *latchPort, uint16_t latchPin)
{
    hspi_ptr = hspi;
    latch_port = latchPort;
    latch_pin = latchPin;

    // Initialize latch pin
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = latch_pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(latch_port, &GPIO_InitStruct);

    // Set latch high initially
    HAL_GPIO_WritePin(latch_port, latch_pin, GPIO_PIN_SET);

    // Clear display
    for (int i = 0; i < NUM_DIGITS; i++) {
        digits_buf[i] = SEG_BLANK;
    }
    current_digit = 0;
}

/* Set individual digits */
void DISP_SetDigits(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
    digits_buf[0] = d0;
    digits_buf[1] = d1;
    digits_buf[2] = d2;
    digits_buf[3] = d3;
}

/* Set temperature with correct formatting */
void DISP_SetTemperature(float temperature)
{
    // Clear display buffer
    for (int i = 0; i < NUM_DIGITS; i++) {
        digits_buf[i] = SEG_BLANK;
    }

    // Limit temperature range
    if (temperature < 0.0f) temperature = 0.0f;
    if (temperature > 999.9f) temperature = 999.9f;

    // Convert to tenths
    uint16_t temp_tenths = (uint16_t)(temperature * 10.0f + 0.5f);

    // Extract digits
    uint8_t hundreds = (temp_tenths / 1000) % 10;         // e.g., 2 for 258.2
    uint8_t tens = (temp_tenths / 100) % 10;       // e.g., 5 for 258.2
    uint8_t units = (temp_tenths / 10) % 10;       // e.g., 8 for 258.2
    uint8_t tenths = temp_tenths % 10;             // e.g., 2 for 258.2

    // Determine display format
    if (temperature < 10.0f) {
        /* Format: X.Y (e.g., 5.6)
           Display: " 5.6" (digit positions: 3,2,1,0)
           digits_buf[3] = leftmost, digits_buf[0] = rightmost */
        digits_buf[3] = SEG_BLANK;      // Leftmost: blank
        digits_buf[2] = SEG_BLANK;          // Second: units digit (e.g., 5)
        digits_buf[1] = units;         // Third: tenths digit (e.g., 6)
        digits_buf[0] = tenths;      // Rightmost: blank

        // Add decimal point to units digit
        digits_buf[1] = set_decimal_point(units);
    }
    else if (temperature < 100.0f) {
        /* Format: XX.Y (e.g., 28.5)
           Display: "28.5" */
        digits_buf[3] = SEG_BLANK ;           // Leftmost: tens digit (e.g., 2)
        digits_buf[2] = tens;          // Second: units digit (e.g., 8)
        digits_buf[1] = units;         // Third: tenths digit (e.g., 5)
        digits_buf[0] = tenths;      // Rightmost: blank

        // Add decimal point to units digit
        digits_buf[1] = set_decimal_point(units);
    }
    else {
        /* Format: XXX.Y (e.g., 258.2)
           Display: "258.2" */
        if (hundreds > 0) {
            // Three-digit number (100-999)
            digits_buf[3] = hundreds;   // Leftmost: hundreds digit (e.g., 2)
            digits_buf[2] = tens;       // Second: tens digit (e.g., 5)
            digits_buf[1] = units;      // Third: units digit (e.g., 8)
            digits_buf[0] = tenths;     // Rightmost: tenths digit (e.g., 2)

            // Add decimal point to units digit
            digits_buf[1] = set_decimal_point(units);
        } else {
            // Two-digit number with tenths (100.0-199.9)
            digits_buf[3] = SEG_BLANK;  // Leftmost: blank
            digits_buf[2] = tens;       // Second: tens digit
            digits_buf[1] = units;      // Third: units digit
            digits_buf[0] = tenths;     // Rightmost: tenths digit

            // Add decimal point to units digit
            digits_buf[1] = set_decimal_point(units);
        }
    }
}

/* Test pattern */
void DISP_TestPattern(void)
{
    // Show "8.8.8.8" pattern
    digits_buf[3] = set_decimal_point(SEG_8);
    digits_buf[2] = set_decimal_point(SEG_8);
    digits_buf[1] = set_decimal_point(SEG_8);
    digits_buf[0] = SEG_8;
}

/* Set brightness */
void DISP_SetBrightness(uint8_t percent)
{
    if (percent == 0) {
        refresh_interval_ms = 0;  // Display off
    } else {
        if (percent > 100) percent = 100;
        // Map 1-100% to 1-5ms refresh interval
        refresh_interval_ms = 1 + (uint16_t)((4 * percent) / 100);
    }
}

/* Send data to shift registers */
static void send_to_display(uint8_t segments, uint8_t digit_select)
{
    uint8_t tx_data[2];

    // IMPORTANT: Try both byte orders if display doesn't work!
    if (SPI_BYTE_ORDER == 0) {
        tx_data[0] = segments;     // Segment data first
        tx_data[1] = digit_select; // Digit select second
    } else {
        tx_data[0] = digit_select; // Digit select first
        tx_data[1] = segments;     // Segment data second
    }

    // Latch pulse
    HAL_GPIO_WritePin(latch_port, latch_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(hspi_ptr, tx_data, 2, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(latch_port, latch_pin, GPIO_PIN_SET);
}

/* Refresh display - call this in main loop */
void DISP_RefreshOnce(void)
{
    if (hspi_ptr == NULL) return;

    // Timing control
    uint32_t current_time = HAL_GetTick();
    if ((current_time - last_refresh_time) < refresh_interval_ms) {
        return;
    }
    last_refresh_time = current_time;

    // Get segments for current digit
    uint8_t segments = get_segments(digits_buf[current_digit]);

    // Build digit select byte
    uint8_t digit_select;

    // IMPORTANT: Try different digit mappings if digits appear wrong!

    // Option A: Digit 0 = leftmost (most common)
    digit_select = (1 << current_digit);

    // Option B: Digit 0 = rightmost (if digits appear reversed)
    // digit_select = (1 << (NUM_DIGITS - 1 - current_digit));

    // Option C: Custom mapping (adjust as needed)
    // uint8_t digit_map[NUM_DIGITS] = {0x01, 0x02, 0x04, 0x08};
    // digit_select = digit_map[current_digit];

    // Invert if digits are active low
    if (!DIGIT_ACTIVE_HIGH) {
        digit_select = ~digit_select;
    }

    // Send to display
    send_to_display(segments, digit_select);

    // Move to next digit
    current_digit = (current_digit + 1) % NUM_DIGITS;
}
