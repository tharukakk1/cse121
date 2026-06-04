#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

// Hardware Configuration
#define MY_ADC_CHANNEL      ADC_CHANNEL_3   // GPIO 3 on Rust-2
#define THRESHOLD_VAL       60              // Calibrated photodiode threshold

// --- LAB 5.3 SPEED TEST CONFIGURATION ---
#define TARGET_UNIT_MS      20   // Adjust this step-by-step (e.g., 200, 100, 50, 40)
#define SAMPLING_PERIOD_MS  10    // 10ms sampling provides crisp precision at higher speeds

// Dynamically calculated boundaries based on chosen testing unit
#define DOT_MAX_MS          (TARGET_UNIT_MS * 1.5)
#define CHAR_GAP_MIN_MS     (TARGET_UNIT_MS * 2.0)
#define WORD_GAP_MIN_MS     (TARGET_UNIT_MS * 5.0)
#define FULL_STOP_MS        (TARGET_UNIT_MS * 8.0)

static const char *TAG = "MORSE_SYSTEM";

char morse_buffer[8] = {0};
int morse_idx = 0;

char decode_morse_char(const char* morse) {
    if (strcmp(morse, ".-") == 0) return 'A';
    if (strcmp(morse, "-...") == 0) return 'B';
    if (strcmp(morse, "-.-.") == 0) return 'C';
    if (strcmp(morse, "-..") == 0) return 'D';
    if (strcmp(morse, ".") == 0) return 'E';
    if (strcmp(morse, "..-.") == 0) return 'F';
    if (strcmp(morse, "--.") == 0) return 'G';
    if (strcmp(morse, "....") == 0) return 'H';
    if (strcmp(morse, "..") == 0) return 'I';
    if (strcmp(morse, ".---") == 0) return 'J';
    if (strcmp(morse, "-.-") == 0) return 'K';
    if (strcmp(morse, ".-..") == 0) return 'L';
    if (strcmp(morse, "--") == 0) return 'M';
    if (strcmp(morse, "-.") == 0) return 'N';
    if (strcmp(morse, "---") == 0) return 'O';
    if (strcmp(morse, ".--.") == 0) return 'P';
    if (strcmp(morse, "--.-") == 0) return 'Q';
    if (strcmp(morse, ".-.") == 0) return 'R';
    if (strcmp(morse, "...") == 0) return 'S';
    if (strcmp(morse, "-") == 0) return 'T';
    if (strcmp(morse, "..-") == 0) return 'U';
    if (strcmp(morse, "...-") == 0) return 'V';
    if (strcmp(morse, ".--") == 0) return 'W';
    if (strcmp(morse, "-..-") == 0) return 'X';
    if (strcmp(morse, "-.--") == 0) return 'Y';
    if (strcmp(morse, "--..") == 0) return 'Z';
    if (strcmp(morse, ".----") == 0) return '1';
    if (strcmp(morse, "..---") == 0) return '2';
    if (strcmp(morse, "...--") == 0) return '3';
    if (strcmp(morse, "....-") == 0) return '4';
    if (strcmp(morse, ".....") == 0) return '5';
    if (strcmp(morse, "-....") == 0) return '6';
    if (strcmp(morse, "--...") == 0) return '7';
    if (strcmp(morse, "---..") == 0) return '8';
    if (strcmp(morse, "----.") == 0) return '9';
    if (strcmp(morse, "-----") == 0) return '0';
    return '?';
}

void process_letter() {
    if (morse_idx > 0) {
        morse_buffer[morse_idx] = '\0';
        char c = decode_morse_char(morse_buffer);
        printf("%c", c);
        fflush(stdout);
        morse_idx = 0; 
    }
}

void app_main(void)
{
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, MY_ADC_CHANNEL, &config));

    int adc_raw;
    int current_state = 0; // 0 = Dark, 1 = Light
    uint32_t state_duration_ms = 0;
    
    // Gated initialization flag
    bool has_started = false;

    ESP_LOGI(TAG, "Gated System Armed. Awaiting initial flash sequence from Pi...");

    while (1) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, MY_ADC_CHANNEL, &adc_raw));
        int sample_state = (adc_raw > THRESHOLD_VAL) ? 1 : 0;

        // If we haven't seen any light yet, look exclusively for the gating trigger
        if (!has_started) {
            if (sample_state == 1) {
                has_started = true;
                current_state = 1;
                state_duration_ms = SAMPLING_PERIOD_MS;
                printf("\n[Transmission Detected] Parsing: ");
                fflush(stdout);
            }
            vTaskDelay(pdMS_TO_TICKS(SAMPLING_PERIOD_MS));
            continue;
        }

        // Active State Machine
        if (sample_state == current_state) {
            state_duration_ms += SAMPLING_PERIOD_MS;
            
            // Evaluated during real-time darkness loops
            if (current_state == 0) {
                // 1. Dynamic character spacing interval boundary
                // Triggers right after the minimum character gap time has passed
                if (state_duration_ms == (CHAR_GAP_MIN_MS + SAMPLING_PERIOD_MS) && morse_idx > 0) {
                    process_letter();
                }
                // 2. Dynamic word spacing interval boundary
                if (state_duration_ms == WORD_GAP_MIN_MS) {
                    printf(" ");
                    fflush(stdout);
                }
                // 3. Dynamic absolute full stop timeout boundary
                if (state_duration_ms >= FULL_STOP_MS) {
                    printf(" [Full Stop]\n");
                    fflush(stdout);
                    has_started = false; // Disarm and re-enter armed waiting mode
                }
            }
        } else {
            // Signal Transition Boundary
            if (current_state == 1) {
                // Light pulse finished -> Register symbol character element using dynamic threshold
                if (state_duration_ms < DOT_MAX_MS) {
                    if (morse_idx < 7) morse_buffer[morse_idx++] = '.';
                } else {
                    if (morse_idx < 7) morse_buffer[morse_idx++] = '-';
                }
            } else {
                // Dark pulse finished (light turned back on before word gap finished)
                // If it was long enough to finish a letter but hasn't been printed yet, print it
                if (state_duration_ms >= CHAR_GAP_MIN_MS && morse_idx > 0) {
                    process_letter();
                }
            }

            current_state = sample_state;
            state_duration_ms = SAMPLING_PERIOD_MS;
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLING_PERIOD_MS));
    }
}