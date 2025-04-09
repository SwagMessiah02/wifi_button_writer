#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"
#include "hardware/adc.h"
#include "lwip/tcp.h"
#include <string.h>
#include <stdio.h>

#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define SERVER_IP "" 
#define SERVER_PORT 5000
#define BTA 5

struct tcp_pcb *client;

struct Pos {
    uint x_pos;
    uint y_pos;
};

static err_t tcp_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err);
static err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len);
void enviar_dados();
const char* posicao(struct Pos pos);
float ler_temperatura();
void setup();

int main() {
    setup();

    // Inicia a comunicação via wi-fi
    if (cyw43_arch_init()) {
        printf("Erro ao iniciar Wi-Fi.\n");
        return 1;
    }

    // Habilita o modo estação wi-fi
    cyw43_arch_enable_sta_mode();

    // Tenta iniciar uma conexão com uma rede wi-fi
    printf("Conectando ao Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
        CYW43_AUTH_WPA2_AES_PSK, 20000)) {
        printf("Erro ao conectar-se ao Wi-Fi.\n");
        return 1;
    }

    printf("Wi-Fi conectado!\n");

    client = tcp_new();
    if (!client) {
        printf("Erro ao criar PCB TCP.\n");
        return 1;
    }

    ip_addr_t server_ip;
    ipaddr_aton(SERVER_IP, &server_ip);

    // Tenta estabelecer uma conexão TCP
    err_t err = tcp_connect(client, &server_ip, SERVER_PORT, tcp_connected_callback);
    if (err != ERR_OK) {
        printf("Erro ao conectar: %d\n", err);
        return 1;
    }

    printf("%d\n", TCP_SND_QUEUELEN);

    sleep_ms(1000); // Aguarda um segundo antes de começar a enviar os dados

    while (true) {
        cyw43_arch_poll();
        enviar_dados();
        sleep_ms(1000); 
    }

    cyw43_arch_deinit();

    return 0;
}

void enviar_dados() {
    bool bta_state = gpio_get(BTA);

    // Armazena no buffer o conteúdo a ser enviado ao servido 
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Botao %s Temperatura: %.2f °C\n", 
                    bta_state ? "Solto" : "Pressionado", ler_temperatura());

    // Envia os dados imediatamente ao servidor TCP
    err_t err = tcp_write(client, buffer, strlen(buffer), TCP_WRITE_FLAG_COPY);

    if(err != ERR_OK) {
        printf("Erro ao tentar enfilheirar dados\n");
        tcp_abort(client);
        return;
    }

    if(tcp_output(client) != ERR_OK) {
        tcp_abort(client);
        return;
    }
}

// Função de callback chamada quando uma conexão TCP for estabelecida com um servidor
static err_t tcp_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err != ERR_OK) {
        printf("Erro ao conectar-se com o servidor: %d\n", err); 
        return err;
    }

    printf("Conectado ao servidor\n");

    char mensagem[64]; 
    snprintf(mensagem, sizeof(mensagem), "Cliente conectado na porta: %d IP: %s\n", SERVER_PORT, SERVER_IP);
    err_t wr_err = tcp_write(tpcb, mensagem, strlen(mensagem), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    printf("queuelen: %d\n", tcp_sndqueuelen(tpcb));

    tcp_sent(tpcb, tcp_sent_callback);
 
    return ERR_OK;
}

// Função de callback para confirma o envio dos dados
static err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    printf("%d bytes foram enviados. queuelen: %d.\n", len, tcp_sndqueuelen(tpcb)); 
    return ERR_OK;
}

// Função para a inicialização dos periféricos
void setup() {
    stdio_init_all();

    gpio_init(BTA);
    gpio_set_dir(BTA, GPIO_IN);
    gpio_pull_up(BTA);

    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
}

float ler_temperatura() {
    uint16_t leitura = adc_read(); // Realiza a leitura do sinal analógico
    const float conversor = 3.3f / (1 << 12);
    float votagem = leitura * conversor; // Obtem a voltagem apartir da leitura anterior 
    float temperatura = 27 - (votagem - 0.706) / 0.001721; // Realiza a conversão para temperatura em graus Celsius 

    return temperatura;
}

