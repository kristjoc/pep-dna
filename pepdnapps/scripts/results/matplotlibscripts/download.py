import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import (FixedFormatter, FormatStrFormatter, FixedLocator)
import matplotlib.ticker as mtick

data = np.genfromtxt('../download.dat')
labels = data[:, 0]
tcpNoTSO = data[:, 1]
tcpWithTSO = data[:, 3]
gateway = data[:, 5]
rina = data[:, 7]

x = np.arange(len(labels))  # the label locations
width = 0.15  # the width of the bars

fig, ax = plt.subplots(figsize=(8, 6))
rects1 = ax.bar(x - 2*width + width/2, tcpWithTSO, width, label='TCP with TSO',
                align='center', color='darkgoldenrod')
rects2 = ax.bar(x - width + width/2, tcpNoTSO, width, label='TCP without TSO',
                align='center', color='brown')
rects3 = ax.bar(x + width - width/2, gateway, width, label='RINA-Gateway',
                align='center', color='darkolivegreen')
rects4 = ax.bar(x + 2*width - width/2 , rina, width, label='RINA',
                align='center', color='steelblue')

# Add some text for labels, title and custom x-axis tick labels, etc.
ax.set_ylabel('Flow Completion Time [s]', fontsize=16)
ax.set_xlabel('Bandwidth [Mbps]', fontsize=16)
# ax.set_title('Scores by group and gender')
ax.set_xticks(x)
ax.set_xticklabels(labels)
ax.legend(fontsize=16)
ax.grid(True, which='both', lw=1, ls='--')
ax.set_yscale('log', basey=2)
ax.yaxis.set_major_formatter(mtick.FormatStrFormatter('%d'))
plt.margins(x=0.15)

def autolabel(rects):
    """Attach a text label above each bar in *rects*, displaying its height."""
    for rect in rects:
        height = rect.get_height()
        ax.annotate('{}'.format(height),
                    xy=(rect.get_x() + rect.get_width() / 2, height),
                    xytext=(0, 3),  # 3 points vertical offset
                    textcoords="offset points",
                    ha='center', va='bottom')


# autolabel(rects1)
# autolabel(rects2)

# fig.tight_layout()

#plt.yscale('log')
plt.show()
