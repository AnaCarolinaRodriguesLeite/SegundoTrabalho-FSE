#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "modbus.h"
#include "uart.h"
#include "crc.h"

int escreve_modbus(char subcodigo, void* buffer) {
    unsigned char tx_buffer[20];
    unsigned char* p_tx_buffer;
    int tam;

    p_tx_buffer = &tx_buffer[0];
    *p_tx_buffer++ = 0x01;
    *p_tx_buffer++ = 0x16; //codigo de envio
    *p_tx_buffer++ = subcodigo;
    *p_tx_buffer++ = 1; //código de matricula para a leitura da potencia
    *p_tx_buffer++ = 7;
    *p_tx_buffer++ = 9;
    *p_tx_buffer++ = 2;

    if (subcodigo == 0xD3 || subcodigo == 0xD4) {
        memcpy(&tx_buffer[7], buffer, sizeof(char));
        tam = p_tx_buffer - &tx_buffer[0] + sizeof(char);
    }
    else if (subcodigo == 0xD2) {
        memcpy(&tx_buffer[7], buffer, sizeof(float));
        tam = p_tx_buffer - &tx_buffer[0] + sizeof(float);
    }
    else if(subcodigo == 0xD1){
        memcpy(&tx_buffer[7], buffer, sizeof(int));
        tam = p_tx_buffer - &tx_buffer[0] + sizeof(int);
    }

    short crc = calcula_CRC(tx_buffer, tam);
    memcpy(&tx_buffer[tam], &crc, sizeof(short));

    init();
    uart_write(tx_buffer, tam + sizeof(short));
    close_uart();
    return 0;
}

int read_modbus(char subcodigo, void* buffer) {
    unsigned char tx_buffer[20];
    unsigned char rx_buffer[255];
    unsigned char* p_tx_buffer;

    p_tx_buffer = &tx_buffer[0];
    *p_tx_buffer++ = 0x01;
    *p_tx_buffer++ = 0x23;
    *p_tx_buffer++ = subcodigo;
    *p_tx_buffer++ = 1;
    *p_tx_buffer++ = 7;
    *p_tx_buffer++ = 9;
    *p_tx_buffer++ = 2;

    short crc = calcula_CRC(tx_buffer, (p_tx_buffer - &tx_buffer[0]));
    memcpy(&tx_buffer[7], &crc, sizeof(short));

    init();
    uart_write(tx_buffer, p_tx_buffer - &tx_buffer[0] + sizeof(short));
    usleep(500000);

    int i, cond = 0, tam;
    for (i = 0; i < 5; i++) {
        short rx_crc;
        tam = uart_read(rx_buffer);
        memcpy(&rx_crc, &rx_buffer[tam - 2], sizeof(short));

        if (tam != -1 && calcula_CRC(rx_buffer, tam - 2) == rx_crc) {
            cond = 1;
            break;
        }
    }
    if (cond == 0)
        return -1;

    int* dado = buffer;

    memcpy(dado, &rx_buffer[3], sizeof(float));

    close_uart();
    return tam;
}