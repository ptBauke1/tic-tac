## *Projeto Flip-Clock*

Esse projeto foi desenvolvido para as diciplinas de Eletrônica Analôgica e Digital do curso de Engenharia de Controle e Automação.
O projeto consiste na construção e programação de um relogio do tipo flip-clock utilizando o microcontrolador RP2040 da Raspberry Pi
e motores de passo.

# *Funcionamento do projeto*
Vídeo mostrando o funcionamento do projeto:

https://github.com/user-attachments/assets/318ebe77-a499-42a6-96d7-d46c9cfe8bdc

Imagens do projeto finalizado:

![frente](https://github.com/user-attachments/assets/30ecc2d1-ab9f-4e7a-b42c-a17c184c7978)

![tras](https://github.com/user-attachments/assets/7d9fee23-203a-4dab-b254-7b694ea8ef7a)

# *Como usar*
Para compilar o codigo é necessario navegar até a pasta 'codigo' e criar um diretorio build e exportar
o caminho para seu diretorio 'pico-sdk'

OBS: Esse arquivo não mostra como instalar o SDK da pico, para isso, busque pelo guia oficial disponibilizado pela propria
Raspberry Pi.

```
cd codigo
mkdir build
cd build
export PICO_SDK_PATH=caminho para seu diretorio pico-sdk
```
Depois, basta usar os comandos a baixo para compilar o codigo e gerar o arquivo .uf2, extensão utilizada pelo microcontrolador.
Essa sequência de comandos ira abrir uma janela do explorardor de arquivos onde será possivel encontrar o arquivo.

```
cmake .. -DPICO_BOARD=waveshare_rp2040_zero
make -j8
explorer.exe .
```

O script em Python serve para mandar, via serial, o horario atualizado durante a inicialização do relogio. Para realizar tal ação:

1 - Conecte o microcontrolador no computador;

2 - Execute o script;

3 - Após a execução do script, caso o projeto não esteja sendo alimentado pelo computador, é possivel desconectar o microcontrolador
do computador.

:)