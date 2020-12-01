import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import (FixedFormatter, FormatStrFormatter, FixedLocator)
import matplotlib.ticker as mtick

link_capacity = np.array([1000, 2500, 5000, 7500, 10000])

filename = ('../inc_pyhttping_5000.dat')
imported_data = np.genfromtxt(filename)

tcpNoTSO   = imported_data[:, 0]
yerr_tcpNoTSO   = imported_data[:, 1]
tcpWithTSO = imported_data[:, 2]
yerr_tcpWithTSO = imported_data[:, 3]
gateway    = imported_data[:, 4]
yerr_gateway    = imported_data[:, 5]
x = np.array([0,1,2,3,4,5,6])
my_xticks = ['1KB','8KB','64KB','512KB', '4MB', '32MB', '256MB']

fix, ax = plt.subplots()

plt.xticks(x, my_xticks)
ax.set_title('Link capacity 5Gbps', fontsize=18)
ax.set_yscale('log', basey=2)
ax.set_ylabel('Time [ms]', fontsize=16)
ax.set_xlabel('HTTP body size [B]', fontsize=16)
ax.yaxis.set_major_formatter(mtick.FormatStrFormatter('%d'))
ax.grid(True, which='both', lw=1, ls='--')

plt.errorbar(x, tcpNoTSO, yerr=yerr_tcpNoTSO, ls='-.', marker='s',
             color='darkblue', label='TCP without TSO')

plt.errorbar(x, tcpWithTSO, yerr=yerr_tcpWithTSO, uplims=False, ls='-.',
             marker='^', color='darkorange', label='TCP with TSO')

plt.errorbar(x, gateway, yerr=yerr_gateway, uplims=False, lolims=False,
             ls='-.', marker='o', color='limegreen', label='RINA-IP gateway')

ax.legend(fontsize=16, loc='best')

plt.tight_layout()
plt.show()
