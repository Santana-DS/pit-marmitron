import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("../data/log.csv")

plt.figure(figsize=(12, 8))

plt.plot(df["tempo"], df["referencia"], label="Referência")
plt.plot(df["tempo"], df["velocidade"], label="Velocidade")

plt.xlabel("Tempo (s)")
plt.ylabel("Velocidade")
plt.title("Resposta do PID")
plt.legend()
plt.grid()

plt.figure(figsize=(12, 6))

plt.plot(df["tempo"], df["pwm"], label="PWM")

plt.xlabel("Tempo (s)")
plt.ylabel("PWM")
plt.title("Sinal de Controle")
plt.legend()
plt.grid()

plt.figure(figsize=(12, 6))

plt.plot(df["tempo"], df["erro"], label="Erro")

plt.xlabel("Tempo (s)")
plt.ylabel("Erro")
plt.title("Erro do Sistema")
plt.legend()
plt.grid()

plt.show()