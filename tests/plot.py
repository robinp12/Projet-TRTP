import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

file = 'time_to_sender.csv'
# Effectuer la tache sur chaque fichier csv
plt.figure()

# Ouvrir les fichiers
headercsv = pd.read_csv(file)
df = pd.read_csv(file)
header = headercsv.columns.tolist()[:-1]


# ['extension ', 'size ', 'time_sender ', 'loss ', 'delay ', 'jitter ', 'error ', 'cut ', 'pkt_corrupted']
header[3] = 'Taux de perte '
header[4] = 'Delai en ms '
header[5] = 'Jitter avec delai de 10ms '
header[6] = 'Taux d\'erreur '
header[7] = 'Taux de coupure '
header[8] = 'Paquet corrompu '

loss = df[df['loss '] != 0]
delay = df[df['delay '] != 0]
jitter = df[df['jitter '] != 0]
error = df[df['error '] != 0]
cut = df[df['cut '] != 0]
pkt_corrupted = df[df['pkt_corrupted'] != 0]

plt.plot(loss['loss '], loss['time_sender '])
plt.plot(delay['delay '], delay['time_sender '])
plt.plot(jitter['jitter '], jitter['time_sender '])
plt.plot(error['error '], error['time_sender '])
plt.plot(cut['cut '], cut['time_sender '])
plt.scatter([80,90,50,70,80,90],pkt_corrupted['time_sender '], c='black')
plt.title("Performances sur réseau instable (fichier de 8890 octets)")
plt.ylabel('Temps')
plt.xlabel('Taux de simulation')
plt.gca().set_xticklabels([f'{int(x)}%' for x in plt.gca().get_xticks()])
plt.gca().set_yticklabels([f'{int(x)}s' for x in plt.gca().get_yticks()])
plt.grid()
plt.legend(header[3:])



plt.savefig("perf")
plt.show()
plt.close()