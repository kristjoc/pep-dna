#!/usr/local/bin/python3

from pathlib import Path
home = str(Path.home())
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import (FixedFormatter,
                               FormatStrFormatter,
                               FixedLocator,
                               AutoMinorLocator)
import matplotlib.ticker as mtick

savefig_path = home + '/'
yerr_offset = 0.024

def run_cpu():
    ''' Everything inside this function
    '''
    filename = ('perf_cpu.dat')
    imported_data = np.genfromtxt(filename)

    # Import TCP data
    pure_tcp = np.array([])
    yerr_pure_tcp = np.array([])
    for i in range(7, 14):
        pure_tcp = np.append(pure_tcp, imported_data[i, 0])
        yerr_pure_tcp = np.append(yerr_pure_tcp, imported_data[i, 1])

    # Import TCP-TCP_U data
    user_pep = np.array([])
    yerr_user_pep = np.array([])
    for i in range(14, 21):
        user_pep = np.append(user_pep, imported_data[i, 0])
        yerr_user_pep = np.append(yerr_user_pep, imported_data[i, 1])

    # Import TCP-TCP data
    tcp2tcp = np.array([])
    yerr_tcp2tcp = np.array([])
    for i in range(0, 7):
        tcp2tcp = np.append(tcp2tcp, imported_data[i, 0])
        yerr_tcp2tcp = np.append(yerr_tcp2tcp, imported_data[i, 1])

    x = np.array([0, 1, 2, 3, 4, 5, 6])
    my_xticks = ['100', '200', '500', '1K', '2K', '5K', '10K']

    fig, ax = plt.subplots()

    plt.xticks(x, my_xticks)
    plt.xticks(fontsize=15)
    plt.yticks(fontsize=15)
    plt.ylabel('CPU utilization [%]', fontsize=20)
    plt.xlabel('Number of concurrent connections', fontsize=20)

    ax.ticklabel_format(axis='y', style='sci', scilimits=(0, 100),
                        useOffset=False, useMathText=True)

    ax.xaxis.set_minor_locator(AutoMinorLocator())

    plt.grid(True, which='major', lw=0.65, ls='--', dashes=(3, 7), zorder=0)

    # plot main TCP line
    plt.errorbar(x,
                 pure_tcp,
                 # ls='--',
                 dashes=[4, 6],
                 # marker='x',
                 linewidth=2,
                 color='darkblue',
                 label='TCP',
                 zorder=10)
    # plt.errorbar(x[6:], pure_tcp[6:], yerr=yerr_pure_tcp[6:], color='darkblue',
                 # ls='none', capsize=2, capthick=2, zorder=10)
    # plot TCP errorbars
    plt.errorbar(x - yerr_offset,
                 pure_tcp,
                 yerr=yerr_pure_tcp,
                 color='darkblue',
                 ls='none',
                 capsize=0.75,
                 capthick=0.5,
                 zorder=20)

    # Plot TCP_TCP_U main line
    plt.errorbar(x,
                 user_pep,
                 # ls='--',
                 dashes=[6, 1],
                 # marker='x',
                 linewidth=2,
                 color='firebrick',
                 label='TCP-TCP_U',
                 zorder=10)

    # plot TCP-TCP_U errorbars
    plt.errorbar(x,
                 user_pep,
                 yerr=yerr_user_pep,
                 color='firebrick',
                 ls='none',
                 capsize=0.75,
                 capthick=0.5,
                 zorder=20)

    # Plot TCP-TCP main line
    plt.errorbar(x,
                 tcp2tcp,
                 # ls='-.',
                 # dashes=[4, 3, 1, 3, 1, 3],
                 dashes=[2, 3, 7, 3],
                 # marker='+',
                 linewidth=2,
                 color='darkorange',
                 label='TCP-TCP',
                 zorder=5)
    # plt.errorbar(x, tcp2tcp, yerr=yerr_tcp2tcp, color='darkorange',
                 # ls='none', capsize=2, capthick=1, zorder=5)
    plt.errorbar(x + yerr_offset,
                 tcp2tcp,
                 yerr=yerr_tcp2tcp,
                 color='darkorange',
                 ls='none',
                 capsize=0.75,
                 capthick=0.5,
                 zorder=20)

#     plt.errorbar(x, tcp2rina, yerr=yerr_tcp2rina, uplims=False, lolims=False,
#                  ls='-.', color='limegreen', label='TCP-RINA')

    plt.legend(fontsize=20, loc='best')
    plt.margins(x=0.02)
    plt.tight_layout(pad=0.4, w_pad=0.5, h_pad=1.0)
    plt.savefig(savefig_path + 'cpu.eps',
                format='eps',
                dpi=1200,
                bbox_inches='tight',
                pad_inches=0.025)

def run_mem():
    ''' Everything inside this function
    '''
    filename = ('perf_mem.dat')
    imported_data = np.genfromtxt(filename)

    pure_tcp = np.array([])
    yerr_pure_tcp = np.array([])
    for i in range(7, 14):
        pure_tcp = np.append(pure_tcp, imported_data[i, 0])
        yerr_pure_tcp = np.append(yerr_pure_tcp, imported_data[i, 1])

    tcp2tcp = np.array([])
    yerr_tcp2tcp = np.array([])
    for i in range(0, 7):
        tcp2tcp = np.append(tcp2tcp, imported_data[i, 0])
        yerr_tcp2tcp = np.append(yerr_tcp2tcp, imported_data[i, 1])

    user_pep = np.array([])
    yerr_user_pep = np.array([])
    for i in range(14, 21):
        user_pep = np.append(user_pep, imported_data[i, 0])
        yerr_user_pep = np.append(yerr_user_pep, imported_data[i, 1])

    x = np.array([0, 1, 2, 3, 4, 5, 6])
    my_xticks = ['100', '200', '500', '1K', '2K', '5K', '10K']

    fig, ax = plt.subplots()

    plt.xticks(x, my_xticks)
    plt.xticks(fontsize=15)
    plt.yticks(fontsize=15)
    plt.ylabel('Memory usage [GB]', fontsize=20)
    plt.xlabel('Number of concurrent connections', fontsize=20)

    ax.ticklabel_format(axis='y', style='sci', scilimits=(0, 0),
                        useOffset=True, useMathText=True)

    ax.xaxis.set_minor_locator(AutoMinorLocator())

    plt.grid(True, which='major', lw=0.65, ls='--', dashes=(3, 7), zorder=0)

    plt.errorbar(x,
                 pure_tcp,
                 # ls='--',
                 dashes=[4, 6],
                 linewidth=2,
                 color='darkblue',
                 label='TCP',
                 zorder=10)

    plt.errorbar(x[:4],
                 pure_tcp[:4],
                 yerr=yerr_pure_tcp[:4],
                 color='darkblue',
                 ls='none',
                 capsize=0.75,
                 capthick=0.5,
                 zorder=20)
    plt.errorbar(x[4:] - yerr_offset,
                 pure_tcp[4:],
                 yerr=yerr_pure_tcp[4:],
                 color='darkblue',
                 ls='none',
                 capsize=0.755,
                 capthick=0.755,
                 zorder=20)

# USER-PEP
    plt.errorbar(x,
                 user_pep,
                 # ls='--',
                 dashes=[6, 1],
                 linewidth=2,
                 color='firebrick',
                 label='TCP-TCP_U',
                 zorder=10)

    plt.errorbar(x,
                 user_pep,
                 yerr=yerr_user_pep,
                 color='firebrick',
                 ls='none',
                 capsize=0.75,
                 capthick=0.5,
                 zorder=20)

# TCP-TCP
    plt.errorbar(x,
                 tcp2tcp,
                 # ls='-.',
                 linewidth=2,
                 dashes=[2, 3, 7, 3],
                 color='darkorange',
                 label='TCP-TCP',
                 zorder=10)
    plt.errorbar(x[:4],
                 tcp2tcp[:4],
                 yerr=yerr_tcp2tcp[:4],
                 color='darkorange',
                 ls='none',
                 capsize=0.75,
                 capthick=0.5,
                 zorder=20)

    plt.errorbar(x[4:] + yerr_offset,
                 tcp2tcp[4:],
                 yerr=yerr_tcp2tcp[4:],
                 color='darkorange',
                 ls='none',
                 capsize=0.75,
                 capthick=0.5,
                 zorder=20)

    # plt.errorbar(x, tcp2rina, yerr=yerr_tcp2rina, uplims=False, lolims=False,
    #              ls='-.', color='limegreen', label='TCP-RINA')

    # Change only ax2
    scale_y = 1e6
    ticks_y = mtick.FuncFormatter(lambda x, pos: '{0:g}'.format(x/scale_y))
    ax.yaxis.set_major_formatter(ticks_y)

    plt.legend(fontsize=20, loc='best')
    plt.margins(x=0.02)
    plt.tight_layout(pad=0.4, w_pad=0.5, h_pad=1.0)
    plt.savefig(savefig_path + 'mem.eps',
                format='eps',
                dpi=1200,
                bbox_inches='tight',
                pad_inches=0.025)

def main():
    run_cpu()
    run_mem()


if __name__ == '__main__':
    main()
