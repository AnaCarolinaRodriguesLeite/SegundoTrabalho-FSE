#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#include "modbus.h"
#include "pid.h"
#include "wiringPi.h"
#include "softPwm.h"
#include "bme280.h"
#include "bme_userspace.h"

#define POTENCIOMETRO 0
#define CURVA 1
#define ARQUIVO 1
#define TERMINAL 2
#define RESISTOR 4
#define VENTOINHA 5

struct DadoArquivo {
    int tempo;
    float temp;
}DadoArquivo;

struct DadoArquivo dados[50];
int cont_tempo = -1;
int pos = 0;
int valor_acionamento;
float temp_int = 0;
int ventoinha = -1;
int resistor = -1;


void config_param() {
    double kp, ki, kd;
    printf("Defina o Kp: ");
    scanf("%lf", &kp);
    printf("Defina o Ki: ");
    scanf("%lf", &ki);
    printf("Defina o Kd: ");
    scanf("%lf", &kd);

    pid_configura_constantes(kp, ki, kd);
}

void atualizarValores() {
    char resposta, novos;
    printf("Parâmetros atuais:\n");
    pid_imprime_constantes();
    printf("\n------------------------------------\n\n");
    printf("Deseja configurar novos parâmetros? (s/n)\n");
    scanf("%c", &novos);
    while (1) {

        scanf("%c", &resposta);
        if (resposta == 's' || resposta == 'S') {
            config_param();
            printf("Parametros atualizados!\n");
            break;
        }
        else if (resposta == 'n' || resposta == 'N')
            break;
        else
            printf("Entrada inválida!\n");
    }
}

void configuracaoTemp() {
    float temp_ref;
    printf("Temperatura de refêrencia: ");
    scanf("%f", &temp_ref);

    pid_atualiza_referencia(temp_ref);
    escreve_modbus(0xD2, &temp_ref); //envia sinal de referência
}

void carregaArquivo() {
    FILE* fp = fopen("curva_reflow.csv", "r");
    if (!fp) {
        printf("Não foi possível abrir o arquivo csv!\n");
    }

    char buffer[1024];
    int linha = 0;
    int coluna = 0;
    while (fgets(buffer, 1024, fp)) {
        coluna = 0;
        linha++;

        if (linha == 1)
            continue;

        char* valor = strtok(buffer, ", ");
        while (valor) {
            if (coluna == 0) {
                dados[linha - 2].temp = strtof(valor, NULL);
            }

            if (coluna == 1) {
                dados[linha - 2].temp = strtof(valor, NULL);
            }

            valor = strtok(NULL, ", ");
            coluna++;
        }
    }
    fclose(fp);
}

void abreArquivo(char* arquivo) {
    FILE* fpt;
    fpt = fopen(arquivo, "w+");
    fprintf(fpt, "Data, Hora, Temperatura Interna, Temperatura Externa, Temperatura Usuário, Valor Acionamento\n");
    fclose(fpt);
}

void salvarArquivo(char modo) {
    if (temp_int != 0) {
        FILE* fpt;
        struct tm* data_hora;
        time_t segundos;

        time(&segundos);
        data_hora = localtime(&segundos);

        if (modo == TERMINAL)
            fpt = fopen("terminal.csv", "a");
        else if (modo == CURVA)
            fpt = fopen("curva.csv", "a");
        else
            fpt = fopen("potenciometro.csv", "a");

        fprintf(fpt, "%d/%d/%d,", data_hora->tm_mday, data_hora->tm_mon + 1, data_hora->tm_year + 1900);
        fprintf(fpt, "%d:%d:%d,", data_hora->tm_hour, data_hora->tm_min, data_hora->tm_sec);

        fprintf(fpt, "%.2lf, ", temp_int);

        double temp_ext = get_temperatura();
        double temp_ref = pid_retorna_referencia();

        fprintf(fpt, "%.2lf, %.2lf, %d\n", temp_ext, temp_ref, valor_acionamento);

        fclose(fpt);
    }
}

void controlAmbiente() {
    float valor;
    if (le_modbus(0xC1, &valor) != -1 && valor > 0 && valor != pid_retorna_referencia()) {
        temp_int = valor;
        int sinalControle = pid_controle(temp_int);

        if (sinalControle >= 0) {
            softPwmWrite(RESISTOR, sinalControle);
            softPwmWrite(VENTOINHA, 0);
        }
        else {
            if (sinalControle > -40) {
                sinalControle = -40;
            }
            softPwmWrite(VENTOINHA, sinalControle * -1);
            softPwmWrite(RESISTOR, 0);
        }
        escreve_modbus(0xD1, &sinalControle);
        valor_acionamento = sinalControle;
    }
}

void curvaReflow() {
    conTempo++;

    if (conTempo == dados[pos].tempo) {
        printf("Atualizando temperatura. . .\n");
        printf("Atualizada para --> %.2lf\n", dados[pos].temp);

        pid_atualiza_referencia(dados[pos].temp);
        escreve_modbus(0xD6, &dados[pos].temp);

        pos++;
    }
}

void send_float_control(float control_signal){

    le_modbus(0x16, 0xD2);
    unsigned char message[255];
    memcpy(message, &control_signal, sizeof(float));

    escreve_modbus(message, 4);
    close_uart();
}

void potenciometro() {
    float temPotenciometro;
    if (le_modbus(0xC2, &temPotenciometro) != -1 && temPotenciometro != 0) {
        pid_atualiza_referencia(temPotenciometro);
        escreve_modbus(0xD2, &temPotenciometro);
        //atualizar o valor do potenciomento
        send_float_control(temPotenciometro); //conferir para ver se ta certo
    }
}

void controlar(char modo) {
    char controle = modo;
    escreve_modbus(0xD4, &controle);
    bme_init();

    if (modo == TERMINAL) {
        printf("Controle pela referência dada pelo terminal!\n");
        controle = 1;
        escreve_modbus(0xD4, &controle);
    }
    else if (modo == CURVA)
        printf("Controle por Curva Reflow!\n");
    else
        printf("Controle por potenciometro!\n");

    if (wiringPiSetup() != -1) {
        pinMode(VENTOINHA, OUTPUT);
        ventoinha = softPwmCreate(VENTOINHA, 0, 100);

        pinMode(RESISTOR, OUTPUT);
        resistor = softPwmCreate(RESISTOR, 0, 100);

        while (1) {
            int valor;
            if (read_modbus(0xC3, &valor) != -1) {
                if (valor == 0xA2) {
                    printf("O sistema está sendo desligado...\n");
                    softPwmStop(RESISTOR);
                    softPwmStop(VENTOINHA);
                    resistor = -1;
                    ventoinha = -1;
                    break;
                }
                else if (valor == 0xA3) {
                    modo = RESISTOR;
                    escreve_modbus(0xD4, &modo);
                    if(resistor != -1 && ventoinha != -1){
                        controlAmbiente();
                    }
                    printf("Controle alterado para resistor!\n");

                }
                else if (valor == 0xA5) {
                    modo = CURVA;
                    carregaArquivo();
                    escreve_modbus(0xD4, &modo);
                    if(modo == CURVA){
                        modo = 1;
                        escreve_modbus(0xD4, &modo);
                    }
                    printf("Controle alterado para Curva Reflow!\n");
                }
            }

            if (modo == POTENCIOMETRO) {
                potenciometro();
            }
            else if (modo == CURVA) {
                curvaReflow();
            }

            controlAmbiente();
            salvarArquivo(modo);
        }
    }
    else {
        printf("Não houve incialização da wiringPi!");
    }
}

void menu() {
    int escolha, valor;
    while (1) {
        printf("1 - Parametros\n2 - Definir temperatura de referencia\n3 - Potenciometro/Curva Reflow\n4 - Modo Debug\n");
        scanf("%d", &escolha);

        if (escolha == 1)
            atualizarValores();
        else if (escolha == 2) {
            configuracaoTemp();
            controlar(TERMINAL);
            break;
        }
        else if (escolha == 3) {
            controlar(POTENCIOMETRO);
            break;
        }
        else if(escolha == 4){
            printf("-Modo Terminal-\n");
            printf("Digite a temperatura que deseja: ");
            float temp_ter;
            scanf("%f",&temp_ter);
            if (temp_ter == 0){
                escreve_modbus(0xD4, &temp_ter);
            }
        }
        if (read_modbus(0xC3, &valor) != -1 && valor == 0xA2) {
            printf("O sistema está sendo desligado...\n");
            break;
        }

        sleep(1);
    }
}

void trataSinal(int sig) {

    if (resistor != -1) {
        softPwmWrite(RESISTOR, 0);
        softPwmStop(RESISTOR);
    }
    if (ventoinha != -1) {
        softPwmWrite(VENTOINHA, 0);
        softPwmStop(VENTOINHA);
    }

    bme_stop();
    char estadoSistema = 0;
    escreve_modbus(0xD3, &estadoSistema);
    exit(0);
}

void inicializacao() {
    printf("Para inciar, defina os parametros para o calculo do PID:\n");
    config_param();

    printf("Aguardando o sistema ser ligado...\n");

    abreArquivo("terminal.csv");
    abreArquivo("curva.csv");
    abreArquivo("potenciometro.csv");
    bme_init();

    read_modbus(0xC1, &temp_int);
}

int main(int argc, const char* argv[]) {

    inicializacao();
    signal(SIGINT, trataSinal);

    while (1) {
        int valor;
        if (le_modbus(0xC3, &valor) != -1) {
            if (valor == 0xA1) {
                printf("Ligando o sistema...\n");
                char estadoSistema = 1;
                escreve_modbus(0xD3, &estadoSistema);
                menu();
                estadoSistema = 0;
                escreve_modbus(0xD3, &estadoSistema);
                printf("Aguardando o sistema ser ligado...\n");
            }
        }
        sleep(1);
    }
    return 0;
}