import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import (FixedFormatter, FormatStrFormatter, FixedLocator)

imported_data = np.genfromtxt('../httping.dat')
labels = imported_data[:, 0]
data = imported_data[:, 1]
stddev = imported_data[:, 2]

x = np.arange(len(labels))  # the label locations
width = 0.25  # the width of the bars


fig, ax = plt.subplots(figsize=(6, 6))
rects1 = plt.bar(-0.25, data[0], width, color='coral', yerr=stddev[0],
                 label='Internet', align='center')
rects2 = plt.bar(0.25, data[1], width, color='teal', yerr=stddev[1],
                 label='RINA-Gateway', align='center')

# Add some text for labels, title and custom x-axis tick labels, etc.
plt.ylabel('Time [ms]', fontsize=16)
# ax.set_title('Scores by group and gender')
#plt.ylim(0, 9)
plt.xticks([], [])
plt.yticks(np.arange(0, 24, 4.0))
plt.legend(fontsize=16, loc='upper left')
plt.grid(True, which='both', axis='y', lw=1, ls='--')
plt.margins(x=0.25)

def autolabel(rects):
    """Attach a text label above each bar in *rects*, displaying its height."""
    for rect in rects:
        height = rect.get_height()
        plt.annotate('{}'.format(height),
                     xy=(rect.get_x() + rect.get_width() / 2, height),
                     xytext=(0, -48),  # 3 points vertical offset
                     textcoords="offset points", ha='center', va='bottom',
                     fontsize=18)

autolabel(rects1)
autolabel(rects2)
fig.tight_layout()

plt.show()
