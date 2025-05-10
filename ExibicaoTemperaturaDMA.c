#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include <string.h>

#define CANAL_ADC_TEMPERATURA 4
#define BUFFER_SIZE 1  // Uma amostra por ciclo

const uint I2C_SDA = 14, I2C_SCL = 15;

uint16_t buffer_adc[BUFFER_SIZE];

// Converte valor ADC em temperatura em °C
float ler_adc_para_temperatura(uint16_t valor_adc) {
    const float fator_conversao = 3.3f / (1 << 12); // 12-bit ADC
    float tensao = valor_adc * fator_conversao;
    return 27.0f - (tensao - 0.706f) / 0.001721f;
}

// Desenha texto no display com escala
void ssd1306_draw_string_scaled(uint8_t *buffer, int x, int y, const char *text, int scale) {
    while (*text) {
        for (int dx = 0; dx < scale; dx++)
            for (int dy = 0; dy < scale; dy++)
                ssd1306_draw_char(buffer, x + dx, y + dy, *text);
        x += 6 * scale;
        text++;
    }
}

int main() {
    stdio_init_all();
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(CANAL_ADC_TEMPERATURA);

    // Configura o DMA
    int dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);

    dma_channel_configure(
        dma_chan,
        &cfg,
        buffer_adc,            // destino
        &adc_hw->fifo,         // origem: FIFO do ADC
        BUFFER_SIZE,           // número de transferências
        false                  // não iniciar ainda
    );

    adc_fifo_setup(
        true,     // Enable FIFO
        true,     // Enable DMA
        1,        // DADOS por amostra
        false,    // Não sobrepor
        false     // Sem shift
    );

    // Inicializa I2C e OLED
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init();

    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area);

    while (true) {
        adc_run(true);
        dma_channel_set_read_addr(dma_chan, &adc_hw->fifo, true);

        // Aguarda leitura ser concluída via DMA
        dma_channel_wait_for_finish_blocking(dma_chan);
        adc_run(false);

        float temperatura_celsius = ler_adc_para_temperatura(buffer_adc[0]);

        uint8_t ssd[ssd1306_buffer_length];
        memset(ssd, 0, ssd1306_buffer_length);

        ssd1306_draw_string_scaled(ssd, 40, 20, "WASLEY", 2);

        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%.fC", temperatura_celsius);
        ssd1306_draw_string_scaled(ssd, 50, 40, buffer, 2);

        render_on_display(ssd, &frame_area);

        sleep_ms(500); // intervalo entre leituras
    }

    return 0;
}
