import numpy as np
import matplotlib as plt
import allantools

file = input("Path para leituras IMU (.csv)")

dados = np.loadtxt(file, delimiter=",", skiprows=1)

fs = 200.0 #Frequencia de amostragem de 200 Hz

acx = dados[: , 1]
acy = dados[: , 2]
acz = dados[: , 3]
girx = dados[: , 4]
giry = dados[: , 5]
girz = dados[: , 6]


# Densidade de ruido: É o valor do desvio de Allan exatamente em tau = 1 segundo
# Random Walk (K): É aproximado na inclinação de +0.5 no gráfico, 
# mas uma estimativa prática de engenharia é olhar o valor de tau próximo a 10s ou 100s
# dividindo pelo fator da raiz correspondente, ou localizando o ponto mínimo da curva.
print("Chamando pro acx")
(tau_out_acx, ad_acx, ade_acx, ns_acx) = allantools.oadev(acx, rate=fs, data_type='freq', taus='all')
idx_1s_acx = np.abs(tau_out_acx - 1.0).argmin()
densidade_ruido_acx = ad_acx[idx_1s_acx]
idx_walk_acx = np.abs(tau_out_acx - 10.0).argmin()
random_walk_acx = ad_acx[idx_walk_acx] / np.sqrt(10.0)


print(f"Parâmetros IMU: \n Noise Density Acc (Pior caso entre os eixos): {densidade_ruido_acx:.6e} \n")
print(f"Random Walk Acc (Pior caso entre eixos): {random_walk_acx:.6e}\n")
