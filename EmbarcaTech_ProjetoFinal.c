#include <stdio.h>           // Entrada e saída padrão (printf, scanf)  
#include <stdlib.h>
#include "pico/stdlib.h"     // Biblioteca padrão do Raspberry Pi Pico  
#include "hardware/pio.h"    // Controle de periféricos PIO  
#include "hardware/clocks.h" // Configuração dos clocks do microcontrolador  
#include "pico/bootrom.h"

#include "hardware/i2c.h"    // Comunicação I2C (sensores, displays)  
#include "lib/font.h"        // Fonte para exibição em displays  
#include "lib/ssd1306.h"     // Controle do display OLED SSD1306  

#include "hardware/adc.h"

#include "hardware/pwm.h"

// Biblioteca gerada pelo arquivo .pio durante compilação.
#include "ws2818b.pio.h"

//Protótipos das funções:
void init();
void gpio_irq_handler(uint gpio, uint32_t events);


// Definições de pinos e endereços I2C
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define I2C_ADDR 0x3C   // Endereço do dispositivo I2C

#define LED_COUNT 25
#define LED_PIN 7
#define RED_LED 13    // Pino do LED vermelho
#define BLUE_LED 12     // Pino do LED azul
#define GREEN_LED 11    // Pino do LED verde
#define BUTTON_A 5      // Pino do botão A
#define BUTTON_B 6      // Pino do botão B

// ADC
#define VRX_PIN 26
//Valor padrão 2028
#define VRY_PIN 27
//Valor padrão 1936
#define SW_PIN 22

// Variáveis globais:

typedef struct Vacas{
    bool active;
    char race[20];
    int age;
    int weight;
    float milk;
    float total_milk;
    int milked_cow;
} Vacas;

Vacas vacas[25];


static volatile uint32_t last_time = 0; // Armazena o tempo da última interrupção
ssd1306_t ssd; // Estrutura de controle do display OLED
uint16_t vrx_value;
uint16_t vry_value;
int i;
bool cor = true;
int day = 1;
char msg[10];
int cow_index = 25;
bool login_screen = true;
bool registration_screen = false;
bool information_screen = false;
bool milk_screen = false;
bool milked_cows;
float average_milk = 0;
int reading = 0;



// Definição de pixel GRB
struct pixel_t {
  uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.

// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

/**
 * Inicializa a máquina PIO para controle da matriz de LEDs.
 */
void npInit(uint pin) {

  // Cria programa PIO.
  uint offset = pio_add_program(pio0, &ws2818b_program);
  np_pio = pio0;

  // Toma posse de uma máquina PIO.
  sm = pio_claim_unused_sm(np_pio, false);
  if (sm < 0) {
    np_pio = pio1;
    sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
  }

  // Inicia programa na máquina PIO obtida.
  ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

  // Limpa buffer de pixels.
  for (uint i = 0; i < LED_COUNT; ++i) {
    leds[i].R = 0;
    leds[i].G = 0;
    leds[i].B = 0;
  }
}

/**
 * Atribui uma cor RGB a um LED.
 */
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
  leds[index].R = r;
  leds[index].G = g;
  leds[index].B = b;
}

/**
 * Limpa o buffer de pixels.
 */
void npClear() {
  for (uint i = 0; i < LED_COUNT; ++i)
    npSetLED(i, 0, 0, 0);
}

/**
 * Escreve os dados do buffer nos LEDs.
 */
void npWrite() {
  // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
  for (uint i = 0; i < LED_COUNT; ++i) {
    pio_sm_put_blocking(np_pio, sm, leds[i].G);
    pio_sm_put_blocking(np_pio, sm, leds[i].R);
    pio_sm_put_blocking(np_pio, sm, leds[i].B);
  }
  sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}



int getIndex(int x, int y) {
    // Se a linha for par (0, 2, 4), percorremos da esquerda para a direita.
    // Se a linha for ímpar (1, 3), percorremos da direita para a esquerda.
    if (y % 2 == 0) {
        return 24-(y * 5 + x); // Linha par (esquerda para direita).
    } else {
        return 24-(y * 5 + (4 - x)); // Linha ímpar (direita para esquerda).
    }
}


void init(){
    stdio_init_all(); // Inicializa entrada e saída padrão
    
    // Configuraração do leds
    gpio_init(RED_LED);
    gpio_set_dir(RED_LED, GPIO_OUT);
    gpio_init(GREEN_LED);
    gpio_set_dir(GREEN_LED, GPIO_OUT);
    gpio_init(BLUE_LED);
    gpio_set_dir(BLUE_LED, GPIO_OUT);


    // Configuraração do botão A
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, gpio_irq_handler);


    // Configuraração do botão B
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, gpio_irq_handler);


    // Configuraração do botão SW
    gpio_init(SW_PIN);
    gpio_set_dir(SW_PIN, GPIO_IN);
    gpio_pull_up(SW_PIN);
    gpio_set_irq_enabled_with_callback(SW_PIN, GPIO_IRQ_EDGE_FALL, true, gpio_irq_handler);

    // Configuração da Matriz de leds
    npInit(LED_PIN);
    npClear();
    npWrite(); // Escreve os dados nos LEDs.


    // Configurações do I2C
    i2c_init(I2C_PORT, 100 * 1000); // Inicializa I2C0 a 100kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // SDA
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // SCL
    gpio_pull_up(I2C_SDA); // Habilita pull-up para SDA
    gpio_pull_up(I2C_SCL); // Habilita pull-up para SCL
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, I2C_ADDR, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);


    // Configuraração do ADC
    adc_init();
    adc_gpio_init(VRX_PIN);
    adc_gpio_init(VRY_PIN);
}


// Callback de interrupção para os botões
void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    // Implementação de debounce (ignora eventos em menos de 300ms)
    if (current_time - last_time > 300000) {
        last_time = current_time;
        if (gpio == BUTTON_A) {
            if(login_screen){
                milk_screen = !milk_screen;
                login_screen = !login_screen;
            }
        }else if(gpio == SW_PIN){
            if(registration_screen){ //Na tela de login existe duas opções: CADASTRAR VACAS e INFORMACOES
                if(abs((vrx_value - 2048)) > 150 && (abs((int)vry_value - 1940) > 150)){
                    if(vrx_value<1890){
                        if(vry_value > 2100){
                            reading = 1;
                        }else if(vry_value < 1790){
                            reading = 3;
                        }
                    }else if(vrx_value > 2200){
                        if(vry_value > 2100){
                            reading = 2;
                        }else if(vry_value < 1790){
                            reading = 4;
                        }
                    }
                }
            }
            if(login_screen){ //Na tela de login existe duas opções: CADASTRAR VACAS e INFORMACOES
                if(vry_value > 1800){
                    registration_screen = !registration_screen;
                }else{
                    information_screen = !information_screen;
                }
                login_screen = !login_screen;
            }
        }else if(gpio == BUTTON_B){
            if(registration_screen && vacas[cow_index].age && vacas[cow_index].weight && vacas[cow_index].race[0] != '\0' && vacas[cow_index].milked_cow==day){
                vacas[cow_index].active = 1;
                registration_screen = !registration_screen;
                login_screen = !login_screen;
                cow_index = 25;     
            }else if(login_screen){
                milked_cows = true;
                int cows_actives = 0;
                for(int i = 0 ; i < 25; i++){
                    if(vacas[i].active == true){
                        cows_actives++;
                        if(vacas[i].milked_cow != day){
                            milked_cows = false;
                        }
                    }
                }
                if(milked_cows){
                    if(cows_actives){
                        day++;
                    }
                    float milk;
                    for(int i = 0; i < 25; i++){
                        if(vacas[i].active == true){
                            milk += vacas[i].milk; 
                        }
                    }
                }
            }else if(information_screen){
                cow_index = 25;
                information_screen = !information_screen;
                login_screen = !login_screen;
            }else if(milk_screen){
                milk_screen = !milk_screen;
                if(!milk_screen){
                    gpio_put(BLUE_LED, 0);
                } 
                login_screen = !login_screen;
            }

        }
    }
}


int main(){
    // Configuração dos leds, botões, adc, i2c, pwm
    init();
    
    /* Dados fictícios para exemplo:
    vacas[0] = (Vacas){.active = true, .race = "NELORE", .age = 6, .weight = 130, .milk = 10.0, .total_milk = 10.0, .milked_cow = 1}; // Produção muito baixa
    vacas[1] = (Vacas){.active = true, .race = "JERSEY", .age = 4, .weight = 450, .milk = 25.0, .total_milk = 25.0, .milked_cow = 1};    // Alta produção
    vacas[2] = (Vacas){.active = true, .race = "ANGUS", .age = 5, .weight = 700, .milk = 12.0, .total_milk = 12.0, .milked_cow = 1};    // Produção baixa
    vacas[3] = (Vacas){.active = true, .race = "GUERNSEY", .age = 2, .weight = 500, .milk = 27.0, .total_milk = 27.0, .milked_cow = 1};  // Boa produção
    vacas[4] = (Vacas){.active = true, .race = "SIMMENTAL", .age = 6, .weight = 750, .milk = 22.0, .total_milk = 22.0, .milked_cow = 1}; // Produção moderada
    vacas[5] = (Vacas){.active = true, .race = "NELORE", .age = 4, .weight = 650, .milk = 10.0, .total_milk = 10.0, .milked_cow = 1};   // Produção muito baixa
    vacas[6] = (Vacas){.active = true, .race = "CHAROLAIS", .age = 5, .weight = 720, .milk = 15.0, .total_milk = 15.0, .milked_cow = 1}; // Produção baixa
    vacas[7] = (Vacas){.active = true, .race = "LIMOUSIN", .age = 3, .weight = 610, .milk = 14.0, .total_milk = 14.0, .milked_cow = 1}; // Produção baixa
    vacas[8] = (Vacas){.active = true, .race = "NORMANDE", .age = 6, .weight = 770, .milk = 20.0, .total_milk = 20.0, .milked_cow = 1}; // Produção moderada
    vacas[9] = (Vacas){.active = true, .race = "HEREFORD", .age = 2, .weight = 580, .milk = 13.0, .total_milk = 13.0, .milked_cow = 1}; // Produção baixa
    vacas[10] = (Vacas){.active = true, .race = "BRANGUS", .age = 3, .weight = 640, .milk = 18.0, .total_milk = 18.0, .milked_cow = 1};  // Produção moderada
    vacas[11] = (Vacas){.active = true, .race = "GIROLANDO", .age = 5, .weight = 580, .milk = 28.0, .total_milk = 28.0, .milked_cow = 1}; // Alta produção
    vacas[12] = (Vacas){.active = true, .race = "GIR", .age = 4, .weight = 520, .milk = 26.0, .total_milk = 26.0, .milked_cow = 1};       // Boa produção
    vacas[13] = (Vacas){.active = true, .race = "MONTBELIARDE", .age = 6, .weight = 730, .milk = 21.0, .total_milk = 21.0, .milked_cow = 1}; // Produção moderada
    vacas[14] = (Vacas){.active = true, .race = "PANTANEIRA", .age = 5, .weight = 550, .milk = 16.0, .total_milk = 16.0, .milked_cow = 1}; // Produção baixa
    vacas[15] = (Vacas){.active = true, .race = "SINDI", .age = 3, .weight = 490, .milk = 24.0, .total_milk = 24.0, .milked_cow = 1};    // Boa produção
    vacas[16] = (Vacas){.active = true, .race = "CARACU", .age = 4, .weight = 570, .milk = 19.0, .total_milk = 19.0, .milked_cow = 1};   // Produção moderada
    vacas[17] = (Vacas){.active = true, .race = "TARENTAISE", .age = 5, .weight = 600, .milk = 17.0, .total_milk = 17.0, .milked_cow = 1}; // Produção baixa
    vacas[18] = (Vacas){.active = true, .race = "KERRY", .age = 2, .weight = 480, .milk = 23.0, .total_milk = 23.0, .milked_cow = 1};    // Boa produção
    vacas[19] = (Vacas){.active = true, .race = "DEVON", .age = 6, .weight = 620, .milk = 20.0, .total_milk = 20.0, .milked_cow = 1};    // Produção moderada
    vacas[20] = (Vacas){.active = true, .race = "DEXTER", .age = 4, .weight = 500, .milk = 22.0, .total_milk = 22.0, .milked_cow = 1};    // Produção moderada
    vacas[21] = (Vacas){.active = true, .race = "RED POLL", .age = 3, .weight = 550, .milk = 21.0, .total_milk = 21.0, .milked_cow = 1}; // Produção moderada
    vacas[22] = (Vacas){.active = true, .race = "FRIESIAN", .age = 5, .weight = 620, .milk = 29.0, .total_milk = 29.0, .milked_cow = 1}; // Alta produção
    vacas[23] = (Vacas){.active = true, .race = "AYRSHIRE", .age = 6, .weight = 580, .milk = 26.0, .total_milk = 26.0, .milked_cow = 1}; // Boa produção
    vacas[24] = (Vacas){.active = true, .race = "HOLSTEIN", .age = 3, .weight = 600, .milk = 30.0, .total_milk = 30.0, .milked_cow = 1};  // Alta produção
    */

    while (true){
        adc_select_input(1); // joystick invertido, por isso é necessário usar 1 no lugar de 0
        vrx_value = adc_read();
        adc_select_input(0); // joystick invertido, por isso é necessário usar 0 no lugar de 1
        vry_value = adc_read();

        if(reading != 0){
            switch (reading){
            case 1:
                gpio_put(RED_LED, 1);
                char buffer_age[3];  
                fflush(stdin);
                fgets(buffer_age, sizeof(buffer_age), stdin);  
                vacas[cow_index].age = atoi(buffer_age);
                gpio_put(RED_LED, 0);
                gpio_put(GREEN_LED, 1);
                sleep_ms(2000);
                gpio_put(GREEN_LED, 0);
                reading = 0;
                break;
            case 2:
                gpio_put(RED_LED, 1);
                char buffer_weight[4];  
                fflush(stdin);
                fgets(buffer_weight, sizeof(buffer_weight), stdin);  
                vacas[cow_index].weight = atoi(buffer_weight);
                gpio_put(RED_LED, 0);
                gpio_put(GREEN_LED, 1);
                sleep_ms(2000);
                gpio_put(GREEN_LED, 0);
                reading = 0;
                break;
            case 3:
                gpio_put(RED_LED, 1);
                char buffer_race[10];  
                fflush(stdin); 
                fgets(buffer_race, sizeof(buffer_race), stdin);  
                strcpy(vacas[cow_index].race, buffer_race);
                gpio_put(RED_LED, 0);
                gpio_put(GREEN_LED, 1);
                sleep_ms(2000);
                gpio_put(GREEN_LED, 0);
                reading = 0;
                break;
            case 4:
                gpio_put(RED_LED, 1);
                char buffer_milk[5];  
                fflush(stdin); 
                fgets(buffer_milk, sizeof(buffer_milk), stdin);  
                vacas[cow_index].milk = atof(buffer_milk);
                vacas[cow_index].total_milk += atof(buffer_milk);
                vacas[cow_index].milked_cow = day;
                gpio_put(RED_LED, 0);
                gpio_put(GREEN_LED, 1);
                sleep_ms(2000);
                gpio_put(GREEN_LED, 0);
                reading = 0;
                break;
            default:
                break;
            }
        }
    

        ssd1306_fill(&ssd, !cor);
        

        if (login_screen){
            ssd1306_draw_string(&ssd, "DIA: ", 40, 5);
            itoa(day, msg, 10);
            ssd1306_draw_string(&ssd, msg, 72, 5);
            if(vry_value < 1800){
                ssd1306_draw_border(&ssd, 42, 0, 16);
            }else{
                ssd1306_draw_border(&ssd, 20, 0, 16);
            }
            ssd1306_draw_string(&ssd, "REGISTRAR VACAS", 4, 24);
            ssd1306_draw_string(&ssd, "INFORMACOES", 16, 46);  

            int matriz[5][5][3];
            for(int i = 0, index; i < 5; i++){
                for(int j = 0; j < 5; j++){
                    index = i * 5 + j;
                    if(vacas[index].active == false){
                        matriz[i][j][0] = 0; //R
                        matriz[i][j][1] = 0; //G
                        matriz[i][j][2] = 0; //B
                    }else{
                        matriz[i][j][0] = 0; //R
                        matriz[i][j][1] = 10; //G
                        matriz[i][j][2] = 0; //B
                    }
                }
            }

            for(int linha = 0; linha < 5; linha++){
                for(int coluna = 0; coluna < 5; coluna++){
                    int posicao = getIndex(linha, coluna);
                    npSetLED(posicao, matriz[coluna][linha][0], matriz[coluna][linha][1], matriz[coluna][linha][2]);
                }
            }
            npWrite();
        }
    

        if(registration_screen){
            if(cow_index>24){
                ssd1306_draw_string(&ssd, "ESCOLHA UMA", 20, 20);
                ssd1306_draw_string(&ssd, "VACA:", 35, 40);
                ssd1306_send_data(&ssd); // Atualiza o display
                
                gpio_put(RED_LED, 1);
                char buffer_cow[3];  
                fflush(stdin);  // Limpa o buffer de entrada
                fgets(buffer_cow, sizeof(buffer_cow), stdin);  
                cow_index = atoi(buffer_cow)-1;
                gpio_put(RED_LED, 0);
                gpio_put(GREEN_LED, 1);
                sprintf(msg, "%d", cow_index+1);
                ssd1306_draw_string(&ssd, msg, 75, 40);
                ssd1306_send_data(&ssd); // Atualiza o display  
                sleep_ms(2000);
                gpio_put(GREEN_LED, 0);
            }
            ssd1306_draw_string(&ssd, "VACA:", 40, 2);
            sprintf(msg, "%d", cow_index+1);
            ssd1306_draw_string(&ssd, msg, 80, 2);
            if(abs((vrx_value - 2048)) > 150 && (abs((int)vry_value - 1940) > 150)){
                if(vrx_value<1890){
                    if(vry_value > 2100){
                        ssd1306_draw_border(&ssd, 17, 0, 7);
                    }else if(vry_value < 1790){
                        ssd1306_draw_border(&ssd, 40, 0, 7);
                    }
                }else if(vrx_value > 2200){
                    if(vry_value > 2100){
                        ssd1306_draw_border(&ssd, 17, 8, 16);
                    }else if(vry_value < 1790){
                        ssd1306_draw_border(&ssd, 40, 8, 16);
                    }
                    
                }
            }
            ssd1306_draw_string(&ssd, "IDADE", 8, 22);
            ssd1306_draw_string(&ssd, "PESO", 80, 22);
            ssd1306_draw_string(&ssd, "RACA", 14, 45);
            ssd1306_draw_string(&ssd, "LEITE", 77, 45);
        }
        
        
        if(information_screen){
            if(cow_index>24){
                ssd1306_draw_string(&ssd, "ESCOLHA UMA", 20, 20);
                ssd1306_draw_string(&ssd, "VACA:", 35, 40);
                ssd1306_send_data(&ssd); // Atualiza o display
                gpio_put(RED_LED, 1);
                char buffer_cow[3];  
                fflush(stdin);  // Limpa o buffer de entrada
                fgets(buffer_cow, sizeof(buffer_cow), stdin);  
                cow_index = atoi(buffer_cow)-1;
                gpio_put(RED_LED, 0);
                gpio_put(GREEN_LED, 1);
                sprintf(msg, "%d", cow_index+1);
                ssd1306_draw_string(&ssd, msg, 75, 40);
                ssd1306_send_data(&ssd); // Atualiza o display  
                sleep_ms(2000);
                gpio_put(GREEN_LED, 0);
            }
            ssd1306_draw_string(&ssd, "VACA:", 40, 2);
            sprintf(msg, "%d", cow_index+1);
            ssd1306_draw_string(&ssd, msg, 80, 2);
            ssd1306_draw_border(&ssd, 11, 0, 6);
            ssd1306_draw_border(&ssd, 11, 7, 16);
            ssd1306_draw_string(&ssd, "AGE:", 1, 16);
            sprintf(msg, "%d", vacas[cow_index].age);
            ssd1306_draw_string(&ssd, msg, 33, 16);
            ssd1306_draw_string(&ssd, "PESO:", 58, 16);
            sprintf(msg, "%d", vacas[cow_index].weight);
            ssd1306_draw_string(&ssd, msg, 97, 16);
            ssd1306_draw_border(&ssd, 27, 0, 16);
            ssd1306_draw_string(&ssd, "RACA:", 4, 32);
            ssd1306_draw_string(&ssd, vacas[cow_index].race, 44, 32);
            ssd1306_draw_border(&ssd, 43, 0, 6);
            ssd1306_draw_border(&ssd, 43, 7, 16);
            ssd1306_draw_string(&ssd, "L:", 4, 48);
            sprintf(msg, "%.1f", vacas[cow_index].milk);
            ssd1306_draw_string(&ssd, msg, 19, 48);
            ssd1306_draw_string(&ssd, "L.D:", 59, 48);
            sprintf(msg, "%.1f", vacas[cow_index].total_milk/day);
            ssd1306_draw_string(&ssd, msg, 92, 48);
            if(vry_value>3600){
                if(cow_index<24){
                    cow_index++;
                    sleep_ms(500);
                }
            }else if(vry_value<400){
                if(cow_index>0){
                    cow_index--;
                    sleep_ms(500);
                }
            }
        }


        if(milk_screen){
            gpio_put(BLUE_LED, 1);
            milked_cows = true;
            for(int i = 0 ; i < 25; i++){
                if(vacas[i].active == true){
                    if(vacas[i].milked_cow != day){
                        milked_cows = false;
                    }
                }
            }
            if(milked_cows){
                int cow_actives = 0;
                average_milk = 0;
                for(int i = 0 ; i < 25; i++){
                    if(vacas[i].active == true){
                        average_milk += vacas[i].total_milk;
                        cow_actives++;
                    }
                }
                if (cow_actives > 0) {
                    average_milk = (average_milk / cow_actives) / day;
                } else {
                    average_milk = 0;
                }
                ssd1306_draw_string(&ssd, "MEDIA DE LEITE:", 4, 20);
                sprintf(msg, "%.1f", average_milk);
                ssd1306_draw_string(&ssd, msg, 45, 40);
                int matriz[5][5][3];
                for(int i = 0, index = 0; i < 5; i++){
                    for(int j = 0; j < 5; j++){
                        index = i * 5 + j;
                        if(vacas[index].active == true){
                            if((vacas[index].total_milk/day) > average_milk || (vacas[index].total_milk/day) == average_milk){
                                matriz[i][j][0] = 0; //R
                                matriz[i][j][1] = 10; //G
                                matriz[i][j][2] = 0; //B
                            }else{
                                matriz[i][j][0] = 10; //R
                                matriz[i][j][1] = 0; //G
                                matriz[i][j][2] = 0; //B
                            }
                        }else{
                            matriz[i][j][0] = 0; //R
                            matriz[i][j][1] = 0; //G
                            matriz[i][j][2] = 0; //B
                        }
                    }   
                }
                for(int linha = 0; linha < 5; linha++){
                    for(int coluna = 0; coluna < 5; coluna++){
                        int posicao = getIndex(linha, coluna);
                        npSetLED(posicao, matriz[coluna][linha][0], matriz[coluna][linha][1], matriz[coluna][linha][2]);
                    }
                }
                npWrite();
                ssd1306_send_data(&ssd); // Atualiza o display
            }else{
                ssd1306_draw_string(&ssd, "INFORME O LEITE", 5, 20);
                ssd1306_draw_string(&ssd, "VACA:", 35, 40);
                int matriz[5][5][3];
                for(int i = 0; i < 25; i++){
                    if(vacas[i].active == true){
                        if(vacas[i].milked_cow != day){
                            sprintf(msg, "%d", i+1);
                            ssd1306_draw_string(&ssd, msg, 75, 40);
                            ssd1306_send_data(&ssd); // Atualiza o display
                            char buffer_milk[5];  
                            fflush(stdin);
                            fgets(buffer_milk, sizeof(buffer_milk), stdin);  
                            vacas[i].milk = atof(buffer_milk);
                            vacas[i].total_milk += atof(buffer_milk);
                            vacas[i].milked_cow = day;
                        }
                    }
                }
            }
        }
        ssd1306_send_data(&ssd); // Atualiza o display
    }
}

