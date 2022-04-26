import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

file = 'time_to_sender.csv'
# Effectuer la tache sur chaque fichier csv
plt.figure()

# Ouvrir les fichiers
headercsv = pd.read_csv(file)
datacsv = pd.read_csv(file, header=None)
header = headercsv.columns.tolist()[:-1]

# Initialisation des vecteurs
plt.plot(headercsv['loss'],headercsv['delay'],label="loss", color='red')
plt.plot(headercsv['jitter'],headercsv['delay'],label="jitter", color='blue')
    
plt.title("Performance en secondes")
plt.ylabel('Temps en sec')
plt.grid()
plt.legend(["loss","jitter"])


# plt.savefig("perf")
plt.show()
plt.close()