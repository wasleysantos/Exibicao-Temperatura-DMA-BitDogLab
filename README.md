# Temperatura do Sensor Interno do Raspberry Pi Pico W

## 📌 Proposta

Este projeto realiza a **leitura da temperatura interna** do microcontrolador **Raspberry Pi Pico W** utilizando comunicação via **DMA (Direct Memory Access)**, permitindo:

- 📡 Captura precisa da temperatura interna (canal ADC 4)
- ⚡ Leitura eficiente com **DMA**, sem sobrecarregar a CPU
- 🧠 Acesso direto à memória, reduzindo o uso de ciclos de processamento
- 🖥️ Exibição dos dados em um **display OLED via I2C**

## 🛠️ Tecnologias e recursos utilizados

- BitDogLab
- Linguagem C com SDK oficial da Raspberry Pi
- ADC interno (canal 4) para temperatura
- Comunicação DMA para leitura de ADC
- Display OLED com driver SSD1306 (via I2C)




![image](https://github.com/user-attachments/assets/aca4180f-c47d-4ed6-b242-c4222af8e9d7)
