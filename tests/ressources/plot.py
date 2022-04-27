import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

file = 'loop_stats.csv'
# Effectuer la tache sur chaque fichier csv
plt.figure()

# Ouvrir les fichiers
df = pd.read_csv(file)



# ['extension', 'size', 'time_sender', 'loss', 'delay', 'jitter', 'error', 'cut', 'pkt_corrupted']

loss = df[df['loss'] != 0]
delay = df[df['delay'] != 0]
jitter = df[df['jitter'] != 0]
error = df[df['error'] != 0]
cut = df[df['cut'] != 0]
pkt_corrupted = df[df['pkt_corrupted'] != 0]

plt.plot(delay['delay'], delay['time_sender'])
plt.plot(jitter['jitter'], jitter['time_sender'])
plt.plot(loss['loss'], loss['time_sender'])


plt.plot(error['error'], error['time_sender'])
plt.plot(cut['cut'], cut['time_sender'])
# plt.scatter([80,90,50,70,80,90],pkt_corrupted['time_sender'], c='black')
plt.title("Performances sur réseau instable")
plt.ylabel('Temps')


# plt.gca().set_xticklabels([f'{int(x)}%' for x in plt.gca().get_xticks()])
# plt.gca().set_yticklabels([f'{int(x)}s' for x in plt.gca().get_yticks()])
plt.grid()

plt.show()
