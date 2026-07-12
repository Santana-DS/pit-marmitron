import numpy as np
import matplotlib.pyplot as plt
import allantools
import yaml
import os

# --- CONFIGURAÇÕES ---
CSV_FILE = 'mpu_raw_data_truncated.csv'  # Arquivo gerado pela ESP32
FS = 200.0                         # Frequência de amostragem (Hz)
OUTPUT_YAML = 'imu_orbslam3.yaml'  # Arquivo de saída para o SLAM
# ---------------------

def calcular_parametros_eixo(sinal, fs):
    """Calcula Noise Density e Random Walk para um único eixo."""
    # Calcula a Overlapping Allan Deviation
    tau_out, ad, _, _ = allantools.oadev(sinal, rate=fs, data_type='freq', taus='all')
    
    # Noise Density (N): valor do desvio de Allan em tau = 1 segundo
    idx_1s = np.abs(tau_out - 1.0).argmin()
    noise_density = ad[idx_1s]
    
    # Random Walk (K): estimativa prática baseada no ponto em tau = 10s
    idx_10s = np.abs(tau_out - 10.0).argmin()
    random_walk = ad[idx_10s] / np.sqrt(10.0)
    
    return tau_out, ad, noise_density, random_walk

def main():
    if not os.path.exists(CSV_FILE):
        print(f"Erro: O arquivo '{CSV_FILE}' não foi encontrado!")
        return

    print(f"Carregando dados de '{CSV_FILE}'...")
    # Carrega pulando o cabeçalho de texto
    dados = np.loadtxt(CSV_FILE, delimiter=',', skiprows=1)
    
    # Dicionário de eixos com seus respectivos índices no CSV
    eixos = {
        'acc_x':  {'idx': 1, 'nome': 'Acelerômetro X', 'unidade': 'm/s²'},
        'acc_y':  {'idx': 2, 'nome': 'Acelerômetro Y', 'unidade': 'm/s²'},
        'acc_z':  {'idx': 3, 'nome': 'Acelerômetro Z', 'unidade': 'm/s²'},
        'gyro_x': {'idx': 4, 'nome': 'Giroscópio X', 'unidade': 'rad/s'},
        'gyro_y': {'idx': 5, 'nome': 'Giroscópio Y', 'unidade': 'rad/s'},
        'gyro_z': {'idx': 6, 'nome': 'Giroscópio Z', 'unidade': 'rad/s'}
    }
    
    resultados = {}
    
    print("Processando a Variância de Allan para todos os eixos...")
    for chave, info in eixos.items():
        sinal = dados[:, info['idx']]
        tau, ad, nd, rw = calcular_parametros_eixo(sinal, FS)
        resultados[chave] = {'tau': tau, 'ad': ad, 'nd': nd, 'rw': rw, 'info': info}
        print(f" -> {info['nome']}: Noise Density = {nd:.6e} | Random Walk = {rw:.6e}")

    # --- CRITÉRIO DO PIOR CASO ---
    # Seleciona o MAIOR valor encontrado entre os eixos X, Y, Z de cada sensor
    pior_acc_noise = max(resultados['acc_x']['nd'], resultados['acc_y']['nd'], resultados['acc_z']['nd'])
    pior_acc_walk  = max(resultados['acc_x']['rw'], resultados['acc_y']['rw'], resultados['acc_z']['rw'])
    
    pior_gyro_noise = max(resultados['gyro_x']['nd'], resultados['gyro_y']['nd'], resultados['gyro_z']['nd'])
    pior_gyro_walk  = max(resultados['gyro_x']['rw'], resultados['gyro_y']['rw'], resultados['gyro_z']['rw'])

    print("\n==================================================")
    print("MÉTRICAS SELECIONADAS (CRITÉRIO DO PIOR CASO):")
    print(f"IMU.AccNoise:  {pior_acc_noise:.6e}")
    print(f"IMU.AccWalk:   {pior_acc_walk:.6e}")
    print(f"IMU.GyroNoise: {pior_gyro_noise:.6e}")
    print(f"IMU.GyroWalk:  {pior_gyro_walk:.6e}")
    print("==================================================\n")

    # --- GERAR ARQUIVO .YAML PARA O ORB-SLAM3 ---
    config_orbslam3 = {
        'IMU.GyroNoise': float(pior_gyro_noise),
        'IMU.GyroWalk': float(pior_gyro_walk),
        'IMU.AccNoise': float(pior_acc_noise),
        'IMU.AccWalk': float(pior_acc_walk),
        'IMU.Frequency': float(FS)
    }
    
    with open(OUTPUT_YAML, 'w') as yaml_file:
        yaml.dump(config_orbslam3, yaml_file, default_flow_style=False, sort_keys=False)
    print(f"Arquivo de configuração salvo com sucesso em: '{OUTPUT_YAML}'")

    # --- GERAR E SALVAR OS PLOTS EM PDF ---
    print("Gerando gráficos em formato PDF...")
    
    # Plot 1: Acelerômetro
    plt.figure(figsize=(10, 6))
    for eixo in ['acc_x', 'acc_y', 'acc_z']:
        res = resultados[eixo]
        plt.loglog(res['tau'], res['ad'], label=f"{res['info']['nome']} (N={res['nd']:.2e})")
    plt.axvline(x=1.0, color='r', linestyle='--', alpha=0.5, label='Tau = 1s (Noise Density)')
    plt.title("Variância de Allan - Acelerômetro MPU9250")
    plt.xlabel("Tempo de Agrupamento $\\tau$ (s)")
    plt.ylabel("Desvio de Allan $\\sigma(\\tau)$ [m/s²]")
    plt.grid(True, which="both", ls="-", alpha=0.3)
    plt.legend()
    plt.savefig('allan_variance_acelerometro.pdf', bbox_inches='tight')
    plt.close()

    # Plot 2: Giroscópio
    plt.figure(figsize=(10, 6))
    for eixo in ['gyro_x', 'gyro_y', 'gyro_z']:
        res = resultados[eixo]
        plt.loglog(res['tau'], res['ad'], label=f"{res['info']['nome']} (N={res['nd']:.2e})")
    plt.axvline(x=1.0, color='r', linestyle='--', alpha=0.5, label='Tau = 1s (Noise Density)')
    plt.title("Variância de Allan - Giroscópio MPU9250")
    plt.xlabel("Tempo de Agrupamento $\\tau$ (s)")
    plt.ylabel("Desvio de Allan $\\sigma(\\tau)$ [rad/s]")
    plt.grid(True, which="both", ls="-", alpha=0.3)
    plt.legend()
    plt.savefig('allan_variance_giroscopio.pdf', bbox_inches='tight')
    plt.close()

    print("Gráficos salvos: 'allan_variance_acelerometro.pdf' e 'allan_variance_giroscopio.pdf'")

if __name__ == '__main__':
    main()
