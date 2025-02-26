# EmbarcaTech_ProjetoFinal

## 📌 DESCRIÇÃO
Este projeto é um sistema de registro monitoramento de vacas leiteiras. Utilizando um microcontrolador Raspberry Pi Pico, o sistema integra diversas tecnologias, como display OLED, matriz de LEDs, leitura de sensores via ADC e comunicação I2C, para permitir o cadastro, consulta e atualização dos dados das vacas, incluindo informações como raça, idade, peso e produção de leite.

---

## 🎯 OBJETIVOS
✅ Compreender e utilizar o ADC do RP2040 para leitura de sensores analógicos (joystick).  
✅ Integrar comunicação I2C para controle do display OLED SSD1306.  
✅ Controlar uma matriz de LEDs WS2818B via PIO para exibição de status.  
✅ Implementar uma interface interativa com múltiplas telas (login, registro, informações e atualização de leite).  
✅ Realizar o registro de dados de vacas (idade, peso, raça e produção de leite).  
✅ Aplicar interrupções (IRQ) com debouncing para tratamento dos botões de interação.  


---

## 🛠️ PRÉ-REQUISITOS

- 🛠️ HARDWARE NECESSÁRIO:
  
  - **Placa de Desenvolvimento:** BitDogLab (RP2040/Raspberry Pi Pico)
  - **Joystick:**
    - VRX conectado ao GPIO 26
    - VRY conectado ao GPIO 27
    - Botão (SW) conectado ao GPIO 22
  - **LEDs:**
    - LED Vermelho conectado ao GPIO 13
    - LED Verde conectado ao GPIO 11
    - LED Azul conectado ao GPIO 12
  - **Botões:**
    - Botão A conectado ao GPIO 5
    - Botão B conectado ao GPIO 6
  - **Matriz de LEDs WS2818B:**
    - 25 LEDs conectados ao GPIO 7
  - **Display OLED SSD1306:**
    - SDA no GPIO 14
    - SCL no GPIO 15

  
- 🖥 SOFTWARE NECESSÁRIO:
  - Raspberry Pi Pico SDK configurado.  
  - CMake para compilação.  
  - VS Code com a extensão Raspberry Pi Pico. 

---

## 🚀 COMO EXECUTAR

1️⃣ **Clone este repositório:**

       git clone https://github.com/gustavo-ferreirasantos/EmbarcaTech_ProjetoFinal

2️⃣ Abra o projeto no VS Code e importe.

3️⃣ Compile e carregue o código na BitDogLab usando o SDK do Raspberry Pi Pico.

---

## 🎥 Vídeo de demonstração
🔗 <u>[Assista aqui](https://www.youtube.com/watch?v=krt7bSanfbo)</u>
