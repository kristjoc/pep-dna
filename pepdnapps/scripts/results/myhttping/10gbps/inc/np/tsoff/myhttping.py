#!/opt/local/bin/python3.9

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import (FixedFormatter, FormatStrFormatter, FixedLocator)
import matplotlib.ticker as mtick
from pathlib import Path
home = str(Path.home())

savefig_path = home + '/'
yerr_offset = 0.0255

def run():
    ''' Everything inside this function
    '''
    # link_capacity = np.array([1000, 2500, 5000, 7500, 10000])

    filename = ('myhttping.dat')
    imported_data = np.genfromtxt(filename)

    pure_tcp = np.array([])
    yerr_pure_tcp = np.array([])
    for i in range(0, 7):
        pure_tcp = np.append(pure_tcp, imported_data[i, 0])
        yerr_pure_tcp = np.append(yerr_pure_tcp, imported_data[i, 1])

    user_pep = np.array([])
    yerr_user_pep = np.array([])
    for i in range(7, 14):
        user_pep = np.append(user_pep, imported_data[i, 0])
        yerr_user_pep = np.append(yerr_user_pep, imported_data[i, 1])

    tcp2tcp = np.array([])
    yerr_tcp2tcp = np.array([])
    for i in range(14, 21):
        tcp2tcp = np.append(tcp2tcp, imported_data[i, 0])
        yerr_tcp2tcp = np.append(yerr_tcp2tcp, imported_data[i, 1])

    tcp2rina = np.array([])
    yerr_tcp2rina = np.array([])
    for i in range(21, 28):
        tcp2rina = np.append(tcp2rina, imported_data[i, 0])
        yerr_tcp2rina = np.append(yerr_tcp2rina, imported_data[i, 1])

    x = np.array([0, 1, 2, 3, 4, 5, 6])
    my_xticks = ['1KB', '8KB', '64KB', '512KB', '4MB', '32MB', '256MB']

    fig, ax = plt.subplots()

    plt.xticks(x, my_xticks, fontsize=15)
    plt.yscale('symlog', base=3)
    plt.ylabel('Time [ms]', fontsize=20, labelpad=-5)
    plt.yticks(fontsize=18)
    plt.xlabel('HTTP body size [B]', fontsize=20)
    ax.yaxis.set_major_formatter(mtick.FormatStrFormatter('%d'))
    plt.xticks(fontsize=15)

    plt.grid(True, which='major', lw=0.65, ls='--', dashes=(3, 7), zorder=0)

# TCP ------------------------------------------------
    plt.errorbar(x,
                 pure_tcp,
                 linewidth=2,
                 dashes=[4, 6],
                 color='darkblue',
                 label='TCP',
                 zorder=25)

    for i in range(0, 7):
        if (i < 3 or i > 5):
            tmp = yerr_offset
        else:
            tmp = 0

        plt.errorbar(x[i] - tmp,
                     pure_tcp[i],
                     yerr=yerr_pure_tcp[i],
                     capsize=0.7475,
                     capthick=0.5,
                     color='darkblue',
                     ls='none',
                     zorder=30)

# TCP-TCP_U ------------------------------------------- 
    plt.errorbar(x,
                 user_pep,
                 linewidth=2,
                 dashes=[6, 1],
                 color='firebrick',
                 label='TCP-TCP_U',
                 zorder=10)

    plt.errorbar(x,
                 user_pep,
                 yerr=yerr_user_pep,
                 capsize=0.7475,
                 capthick=0.5,
                 color='firebrick',
                 ls='none',
                 zorder=30)
# TCP-TCP ---------------------------------------------
    plt.errorbar(x,
                 tcp2tcp,
                 linewidth=2,
                 dashes=[2, 3, 7, 3],
                 color='darkorange',
                 label='TCP-TCP',
                 zorder=10)

    for i in range(0, 7):
        plt.errorbar(x[i] + yerr_offset,
                     tcp2tcp[i],
                     yerr=yerr_tcp2tcp[i],
                     capsize=0.7475,
                     capthick=0.5,
                     color='darkorange',
                     ls='none',
                     zorder=30)

# TCP2RINA --------------------------------------------
    plt.errorbar(x,
                 tcp2rina,
                 # ls=':',
                 linewidth=2,
                 dashes=[2, 3],
                 color='green',
                 label='TCP-RINA',
                 zorder=10)
    plt.errorbar(x,
                 tcp2rina,
                 yerr=yerr_tcp2rina,
                 capsize=0.7475,
                 capthick=0.5,
                 color='green',
                 ls='none',
                 zorder=30)

    handles, labels = ax.get_legend_handles_labels()
    # remove the errorbars
    handles = [h[0] for h in handles]
    # use them in the legen
    plt.legend(handles, labels, fontsize=20, loc='best')
    plt.tight_layout(pad=0.4, w_pad=0.5, h_pad=1.0)

    # plt.show()
    plt.savefig(savefig_path + '10_inc_np_disabled.eps', format='eps',
                dpi=1200, bbox_inches='tight', pad_inches=0.025)

def main():
    run()


if __name__ == '__main__':
    main()
