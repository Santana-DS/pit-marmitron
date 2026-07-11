import serial
import time
import csv

# --- CONFIGURAÇÕES ---
SERIAL_PORT = '/dev/ttyUSB0'  # Altere para a porta da sua ESP32 (ex: 'COM3' no Windows)
BAUD_RATE = 921600
OUTPUT_FILENAME = 'mpu9250_raw_data.csv'
# ---------------------

def main():
    print(f"Tentando abrir a porta {SERIAL_PORT}...")
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    except Exception as e:
        print(f"Erro ao abrir a porta serial: {e}")
        return

    # Aguarda 2 segundos para o reset automático da ESP32 ao abrir a conexão
    time.sleep(2)
    ser.reset_input_buffer()
    
    print(f"Conectado com sucesso! Gravando dados em '{OUTPUT_FILENAME}'")
    print("MANTENHA A IMU TOTALMENTE IMÓVEL. Pressione Ctrl+C para encerrar...")

    # Abre o arquivo CSV para escrita
    with open(OUTPUT_FILENAME, mode='w', newline='', encoding='utf-8') as arquivo_csv:
        gerador_csv = csv.writer(arquivo_csv)
        
        # Escreve o cabeçalho no arquivo (opcional, mas ajuda na organização)
        # O script do Allantools fornecido anteriormente ignora texto se usarmos np.loadtxt(..., skiprows=1)
        gerador_csv.writerow(['timestamp_esp_ms', 'acc_x', 'acc_y', 'acc_z', 'gyro_x', 'gyro_y', 'gyro_z'])
        
        linhas_gravadas = 0
        t_inicio = time.time()

        try:
            while True:
                if ser.in_waiting > 0:
                    # Lê a linha vinda da ESP32
                    linha_bruta = ser.readline().decode('utf-8', errors='ignore').strip()
                    dados = linha_bruta.split(',')
                    
                    # Verifica se a linha contém exatamente os 7 campos enviados pela ESP32
                    if len(dados) == 7:
                        try:
                            # Converte para float apenas para garantir que o dado não está corrompido
                            linha_convertida = [float(val) for val in dados]
                            
                            # Salva a linha no CSV
                            gerador_csv.writerow(linha_convertida)
                            
                            linhas_gravadas += 1
                            
                            # Mostra o progresso no terminal a cada 2000 linhas (~10 segundos)
                            if linhas_gravadas % 2000 == 0:
                                t_atual = time.time() - t_inicio
                                print(f"[{int(t_atual)}s decorridos] Linhas salvas: {linhas_gravadas}")
                                
                        except ValueError:
                            # Ignora linhas malformadas decorrentes de ruído inicial na Serial
                            continue
                            
        except KeyboardInterrupt:
            t_total = time.time() - t_inicio
            print(f"\n\nGravação interrompida pelo usuário.")
            print(f"Tempo total de gravação: {t_total/3600:.2f} horas")
            print(f"Total de amostras salvas: {linhas_gravadas}")
            print(f"Arquivo '{OUTPUT_FILENAME}' fechado com sucesso!")
        finally:
            ser.close()

if __name__ == '__main__':
    main()
