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

print("Chamando pro acy")
(tau_out_acy, ad_acy, ade_acy, ns_acy) = allantools.oadev(acy, rate=fs, data_type='freq', taus='all')
idx_1s_acy = np.abs(tau_out_acy - 1.0).argmin()
densidade_ruido_acy = ad_acy[idx_1s_acy]
idx_walk_acy = np.abs(tau_out_acy - 10.0).argmin()
random_walk_acy = ad_acy[idx_walk_acy] / np.sqrt(10.0)

(tau_out_acz, ad_acz, ade_acz, ns_acz) = allantools.oadev(acz, rate=fs, data_type='freq', taus='all')
idx_1s_acz = np.abs(tau_out_acz - 1.0).argmin()
densidade_ruido_acz = ad_acz[idx_1s_acz]
idx_walk_acz = np.abs(tau_out_acz - 10.0).argmin()
random_walk_acz = ad_acz[idx_walk_acz] / np.sqrt(10.0)


(tau_out_girx, ad_girx, ade_girx, ns_girx) = allantools.oadev(girx, rate=fs, data_type='freq', taus='all')
idx_1s_girx = np.abs(tau_out_girx - 1.0).argmin()
densidade_ruido_girx = ad_girx[idx_1s_girx]
idx_walk_girx = np.abs(tau_out_girx - 10.0).argmin()
random_walk_girx = ad_girx[idx_walk_girx] / np.sqrt(10.0)

(tau_out_giry, ad_giry, ade_giry, ns_giry) = allantools.oadev(giry, rate=fs, data_type='freq', taus='all')
idx_1s_giry = np.abs(tau_out_giry - 1.0).argmin()
densidade_ruido_giry = ad_giry[idx_1s_giry]
idx_walk_giry = np.abs(tau_out_giry - 10.0).argmin()
random_walk_giry = ad_giry[idx_walk_giry] / np.sqrt(10.0)

(tau_out_girz, ad_girz, ade_girz, ns_girz) = allantools.oadev(girz, rate=fs, data_type='freq', taus='all')
idx_1s_girz = np.abs(tau_out_girz - 1.0).argmin()
densidade_ruido_girz = ad_girz[idx_1s_girz]
idx_walk_girz = np.abs(tau_out_girz - 10.0).argmin()
random_walk_girz = ad_girz[idx_walk_girz] / np.sqrt(10.0)

print(f"Parâmetros IMU: \n Noise Density Acc (Pior caso entre os eixos): {max([densidade_ruido_acx, densidade_ruido_acy, densidade_ruido_acz]):.6e} \n")
print(f"Random Walk Acc (Pior caso entre eixos): {max([random_walk_acx, random_walk_acy, random_walk_acz]):.6e}\n")
print(f"Noise density Gir: {max([densidade_ruido_girx, densidade_ruido_giry, densidade_ruido_girz]):.6e}\n")
print(f"Random Walk Gir: {max([random_walk_girx, random_walk_giry, random_walk_girz]):.6e}\n")
