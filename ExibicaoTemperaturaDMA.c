#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/i2c.h"
#include "ssd1306.h"

#define TEMPERATURA 4
#define BUFFER_SIZE 1

const uint I2C_SDA = 14, I2C_SCL = 15;

uint16_t buffer_adc[BUFFER_SIZE];
uint8_t ssd[ssd1306_buffer_length];
struct render_area frame_area;

float ler_adc_para_temperatura(uint16_t valor_adc) {
    const float fator_conversao = 3.3f / (1 << 12);
    float tensao = valor_adc * fator_conversao;
    return 27.0f - (tensao - 0.706f) / 0.001721f;
}

void ssd1306_draw_string_scaled(uint8_t *buffer, int x, int y, const char *text, int scale) {
    while (*text) {
        for (int dx = 0; dx < scale; dx++) {
            for (int dy = 0; dy < scale; dy++) {
                ssd1306_draw_char(buffer, x + dx, y + dy, *text);
            }
        }
        x += 6 * scale;
        text++;
    }
}

void exibir_temperatura() {
    float temperatura_celsius = ler_adc_para_temperatura(buffer_adc[0]);

    memset(ssd, 0, ssd1306_buffer_length);

    ssd1306_draw_string_scaled(ssd, 0, 20, "TEMPERATURA", 2);

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.1fC", temperatura_celsius);

    int scale = 2;
    int text_width = strlen(buffer) * 6 * scale;
    int pos_x = (ssd1306_width - text_width) / 2;

    ssd1306_draw_string_scaled(ssd, pos_x, 40, buffer, scale);

    render_on_display(ssd, &frame_area);
}

int main() {
    stdio_init_all();
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(TEMPERATURA);

    // Configuração DMA (apenas uma vez)
    int dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);

    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);
    dma_channel_configure(
        dma_chan, &cfg,
        buffer_adc, &adc_hw->fifo,
        BUFFER_SIZE,
        false
    );

    adc_fifo_setup(
        true, true,
        1, false, false
    );

    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init();

    frame_area.start_column = 0;
    frame_area.end_column = ssd1306_width - 1;
    frame_area.start_page = 0;
    frame_area.end_page = ssd1306_n_pages - 1;
    calculate_render_area_buffer_length(&frame_area);

    while (true) {
        adc_run(true);

        // Reinicia DMA corretamente a cada leitura
        dma_channel_configure(
            dma_chan, &cfg,
            buffer_adc, &adc_hw->fifo,
            BUFFER_SIZE,
            true  // Inicia agora
        );

        dma_channel_wait_for_finish_blocking(dma_chan);
        adc_run(false);

        exibir_temperatura();
        sleep_ms(500);
    }

    return 0;
}
