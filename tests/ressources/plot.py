import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

if True:
    df = pd.read_csv("perfect_stats.csv")
    size = df["size"].to_numpy()/1000000
    time = df["time_sender"].to_numpy()
    speed = size/time
    # plt.plot(size, time)
    # plt.plot(size, speed)

    # plt.title("Vitesse de transfert sur réseau parfait")
    # plt.xlabel("Taille du fichier [octets]")
    # plt.ylabel("Temps [s]")
    # plt.show()


    fig, ax1 = plt.subplots()


    ax1.set_xlabel('Taille [Mo]')
    ax1.set_ylabel('Durée de transmission [s]')
    ax1.plot(size, time, c='k', label="Durée")

    ax2 = ax1.twinx()  # instantiate a second axes that shares the same x-axis

    ax2.set_ylabel('Vitesse [Mo/s]')  # we already handled the x-label with ax1
    ax2.plot(size, speed, ls="--", c='g', label="Vitesse")

    print(np.mean(speed))
    print(np.std(speed))
    print(np.max(speed))

    fig.legend(loc="lower right")
    plt.title("Transfert de fichier texte aléatoire sur réseau parfait")
    fig.tight_layout()  # otherwise the right y-label is slightly clipped
    plt.tight_layout()
    plt.show()

if False:
    file = 'loop_stats.csv'

    df = pd.read_csv(file)



    # ['extension', 'size', 'time_sender', 'loss', 'delay', 'jitter', 'error', 'cut', 'pkt_corrupted']

    loss = df[df['loss'] != 0]
    delay = df[df['delay'] != 0]
    jitter = df[df['jitter'] != 0]
    error = df[df['error'] != 0]
    cut = df[df['cut'] != 0]
    pkt_corrupted = df[df['pkt_corrupted'] != 0]
    print(jitter)

    fig, axs = plt.subplots(2,2, constrained_layout=True)
    fig.suptitle("Effet de différents paramètres sur la vitesse de transfert")
    axs[0, 0].plot(jitter["jitter"], jitter["time_sender"])
    axs[0, 0].set_xlabel("Jitter (with 50ms delay) [ms]")
    axs[1, 0].plot(error["error"], error["time_sender"])
    axs[1, 0].set_xlabel("Error rate [%]")
    axs[1, 1].plot(cut["cut"], cut["time_sender"])
    axs[1, 1].set_xlabel("Cut rate [%]")
    axs[0, 1].plot(loss["loss"], loss["loss"])
    axs[0, 1].set_xlabel("Loss rate [%]")

    for ax in axs.flat :
        ax.set(ylabel="Time [s]")
        
    plt.show()