# Segundo Trabalho - FSE

## Desrição

Esse trabalho simula um sistema de controle do forno para soldagem de placas de circuito impresso.

## Uso

- Ao iniciar o programa, será solicitada a configuração dos parâmetros para o controle de temperatura através do PID.   

- QUando o sistema estiver ligado, o você deve escolher uma opção do menu:
    
    - **Parâmetros**: O usuário poderá verificar os parâmetros que serão utilizados no controle PID, Além disso, poderá alterar esses parâmetros.

    - **Definir temperatura de referência**: O usuário pode indicar uma temperatura pelo modo terminal

    - **Potenciometro/Curva Reflow**: O sistema irá iniciar o controle de temperatura pelo modo potenciômetro, permitindo que o usuário altere para o modo de curva reflow.

    - **Modo Debug**

- Quando o sistema é desligado, o programa volta para o estado de espera para o sistema ser ligado novamente. 

## Compilação

Use o comando 

make all

Após

make run