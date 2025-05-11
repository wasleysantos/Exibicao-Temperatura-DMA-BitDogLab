#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include <string.h>

// Canal do sensor de temperatura interno do RP2040
#define TEMPERATURA 4
#define BUFFER_SIZE 1  // Apenas uma amostra por ciclo de leitura

const uint I2C_SDA = 14, I2C_SCL = 15;  // Pinos I2C usados para o display OLED

uint16_t buffer_adc[BUFFER_SIZE];  // Buffer que receberá os dados do ADC via DMA
uint8_t ssd[ssd1306_buffer_length];  // Buffer de vídeo do display OLED
struct render_area frame_area;  // Área do display onde os dados serão desenhados

// Converte o valor lido do ADC em temperatura em Celsius
float ler_adc_para_temperatura(uint16_t valor_adc) {
    const float fator_conversao = 3.3f / (1 << 12); // Conversão para tensão (12 bits)
    float tensao = valor_adc * fator_conversao;
    return 27.0f - (tensao - 0.706f) / 0.001721f;  // Fórmula do datasheet do RP2040
}

// Função para desenhar texto ampliado no display
void ssd1306_draw_string_scaled(uint8_t *buffer, int x, int y, const char *text, int scale) {
    while (*text) {
        for (int dx = 0; dx < scale; dx++) {
            for (int dy = 0; dy < scale; dy++) {
                ssd1306_draw_char(buffer, x + dx, y + dy, *text);
            }
        }
        x += 6 * scale;  // Avança para o próximo caractere
        text++;
    }
}

// Exibe o nome e a temperatura no display
void exibir_temperatura() {
    float temperatura_celsius = ler_adc_para_temperatura(buffer_adc[0]);

    memset(ssd, 0, ssd1306_buffer_length);  // Limpa o buffer do display

    ssd1306_draw_string_scaled(ssd, 40, 20, "WASLEY", 2);  // Escreve o nome no display

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.0fC", temperatura_celsius);  // Formata a temperatura
    ssd1306_draw_string_scaled(ssd, 50, 40, buffer, 2);  // Escreve a temperatura

    render_on_display(ssd, &frame_area);  // Envia o buffer para o display OLED
}

int main() {
    stdio_init_all();  // Inicializa entrada/saída padrão (útil para debug)
    adc_init();  // Inicializa ADC
    adc_set_temp_sensor_enabled(true);  // Ativa sensor de temperatura interno
    adc_select_input(TEMPERATURA);  // Seleciona canal 4 (sensor de temperatura)

    // --- Configuração do DMA para transferir dados do ADC para a RAM ---
    int dma_chan = dma_claim_unused_channel(true);  // Solicita um canal de DMA disponível
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);

    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);  // Transfere 16 bits
    channel_config_set_read_increment(&cfg, false);  // Endereço de leitura fixo (ADC FIFO)
    channel_config_set_write_increment(&cfg, true);  // Endereço de escrita incrementa (buffer)
    channel_config_set_dreq(&cfg, DREQ_ADC);  // Disparo do DMA controlado pelo ADC

    dma_channel_configure(
        dma_chan, &cfg,
        buffer_adc, &adc_hw->fifo,  // Destino e origem
        BUFFER_SIZE,                // Número de amostras
        false                       // Ainda não inicia
    );

    adc_fifo_setup(
        true,  // Ativa FIFO
        true,  // Ativa solicitação via DMA
        1,     // Uma amostra por leitura
        false, // Não sobrescreve
        false  // Sem deslocamento de bits
    );

    // --- Inicialização da comunicação I2C e display OLED ---
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);  // Inicializa I2C1
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init();  // Inicializa display OLED

    // Define área do display a ser atualizada
    frame_area.start_column = 0;
    frame_area.end_column = ssd1306_width - 1;
    frame_area.start_page = 0;
    frame_area.end_page = ssd1306_n_pages - 1;
    calculate_render_area_buffer_length(&frame_area);  // Calcula o tamanho do buffer

    // --- Loop principal ---
    while (true) {
        adc_run(true);  // Inicia conversão ADC
        dma_channel_set_read_addr(dma_chan, &adc_hw->fifo, true);  // Inicia leitura via DMA

        dma_channel_wait_for_finish_blocking(dma_chan);  // Espera DMA concluir
        adc_run(false);  // Para o ADC

        exibir_temperatura();  // Atualiza o display

        sleep_ms(500);  // Aguarda 0,5s antes da próxima leitura
    }

    return 0;
}
