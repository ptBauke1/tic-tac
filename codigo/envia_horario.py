import serial
import time
from datetime import datetime

# Configura a porta serial e a taxa de baud (ajuste 'COM3' para a porta correta)
ser = serial.Serial('COM7', 115200, timeout=1)
time.sleep(2)  # Aguarde a inicialização da porta serial

input("Iniciar...")

horario = datetime.now()
# Dados a serem enviados
data = f"{horario.year}-{horario.month}-{horario.day}"    # Data no formato "YYYY-MM-DD"
hora = f"{horario.hour}:{horario.minute}:{horario.second}"      # Hora no formato "HH:MM:SS"
dia_da_semana = f"3"    # Dia da semana (0 = domingo, 1 = segunda, ..., 6 = sábado)

# Enviar cada string seguida de uma nova linha para separação
ser.write((data + "\n").encode())         # Envia a data
time.sleep(0.1)                           # Pequeno intervalo entre envios

ser.write((hora + "\n").encode())         # Envia a hora
time.sleep(0.1)

ser.write((dia_da_semana + "\n").encode())  # Envia o dia da semana
time.sleep(0.1)

print("Informações enviadas para configurar o RTC:")
print("Data:", data)
print("Hora:", hora)
print("Dia da semana:", dia_da_semana)

# Fechar a porta serial
ser.close()