#!/usr/local/bin/python3

import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
# from matplotlib.ticker import (FixedFormatter, FormatStrFormatter, FixedLocator)
from pathlib import Path
home = str(Path.home())

savefig_path = home + '/'
labels = ['TCP', 'TCP-TCP_U', 'TCP-TCP', 'TCP-RINA']
filename = ('results/fct.dat')
imported_data = np.genfromtxt(filename)

def autolabe(ax, rects):
    '''Attach a text label above each bar in *rects*, displaying its height.
    '''
    for rect in rects:
        height = rect.get_height()
        # height = round(height, 3)
        ax.annotate('{}'.format(height),
                    xy=(rect.get_x() + rect.get_width() / 2, height),
                    xytext=(0, 6),  # 3 points vertical offset
                    textcoords="offset points",
                    ha='center', va='bottom',
                    fontsize=9, zorder=10)

def autolabel(ax, rects):
    '''Attach a text label above each bar displaying its height
    '''
    data_line, capline, barlinecols = rects.errorbar

    for err_segment, rect in zip(barlinecols[0].get_segments(), rects):
        height = err_segment[1][1]  # Use height of error bar

        ax.text(rect.get_x() + rect.get_width() / 2,
                height,
                f'{height:.3f}',
                ha='center', va='bottom',
                fontsize=10, zorder=10)
def run_1g():
    ''' Everything inside this function
    '''
    tso_enabled = [36.286, 36.296, 36.271]
    yerr_tso_enabled = [0.090961, 0.097354603, 0.10887426]
    tso_disabled = [36.289, 36.310, 36.298]
    yerr_tso_disabled = [0.09775081, 0.12880254, 0.067810446]

    x = np.arange(len(labels))  # the label locations
    width = 0.30  # the width of the bars

    fig, ax = plt.subplots()
    rects1 = ax.bar(x - width/2,
                    tso_enabled,
                    width,
                    yerr=yerr_tso_enabled,
                    label='TSO: enabled',
                    color='darkorange',
                    edgecolor='k')

    rects2 = ax.bar(x + width/2,
                    tso_disabled,
                    width,
                    yerr=yerr_tso_disabled,
                    label='TSO: disabled',
                    color='royalblue',
                    edgecolor='k')

    # Add some text for labels, title and custom x-axis tick labels, etc.
    ax.set_ylabel('Flow Time Completion [s]', fontsize=15)
    # ax.set_title('Link Capacity: 1Gbps', fontsize=18)
    ax.set_xticks(x)
    ax.set_yticks(np.arange(30, 39, 2))
    ax.set_ylim(30, 39)
    # ax.set_yticks([0, 32, 34, 36, 38, 64])
    # ax.set_yscale('log', basey=2)
    plt.yticks(fontsize=12)
    ax.set_xticklabels(labels, fontsize=14)
    legend = ax.legend(fontsize=18, loc='upper right')

    ax.grid(True, which='major', axis='y', lw=0.5, ls='--', dashes=(3, 7), zorder=0)

    autolabel(ax, rects1)
    autolabel(ax, rects2)

    fig.tight_layout()

    plt.draw() # Draw the figure so you can find the positon of the legend.

    # Get the bounding box of the original legend
    bb = legend.get_bbox_to_anchor().inverse_transformed(ax.transAxes)

    # Change to location of the legend.
    xOffset = 0.015
    yOffset = 0.0175
    bb.x0 += xOffset
    bb.x1 += xOffset
    bb.y0 += yOffset
    bb.y1 += yOffset
    legend.set_bbox_to_anchor(bb, transform=ax.transAxes)

    plt.savefig(savefig_path + '1gbps.eps',
                format='eps', dpi=1200)

def run_10g():
    ''' Everything inside this function
    '''
    tso_enabled = np.array([])
    yerr_tso_enabled = np.array([])
    for i in range(0, 7, 2):
        tso_enabled = np.append(tso_enabled, imported_data[i, 0]/1000)
        yerr_tso_enabled = np.append(yerr_tso_enabled, imported_data[i, 1]/1000)

    tso_enabled[:] = [round(x, 3) for x in tso_enabled]
    yerr_tso_enabled[:] = [round(x, 3) for x in yerr_tso_enabled]

    tso_disabled = np.array([])
    yerr_tso_disabled = np.array([])
    for i in range(1, 8, 2):
        tso_disabled = np.append(tso_disabled, imported_data[i, 0]/1000)
        yerr_tso_disabled = np.append(yerr_tso_disabled, imported_data[i, 1]/1000)

    tso_disabled[:] = [round(x, 3) for x in tso_disabled]
    yerr_tso_disabled[:] = [round(x, 3) for x in yerr_tso_disabled]

    x = np.array([1, 1.2, 1.4, 1.6])  # the label locations

    # x = np.arange(len(labels))  # the label locations
    width = 0.08  # the width of the bars

    fig, ax = plt.subplots()
    rects1 = ax.bar(x - width/2,
                    tso_enabled,
                    width,
                    yerr=yerr_tso_enabled,
                    label='TSO: enabled',
                    color='darkorange',
                    edgecolor='k',
                    align='center',
                    zorder=10,
                    hatch='//')

    rects2 = ax.bar(x + width/2,
                    tso_disabled,
                    width,
                    yerr=yerr_tso_disabled,
                    label='TSO: disabled',
                    color='royalblue',
                    edgecolor='k',
                    align='center',
                    zorder=10,
                    hatch='xx')

    mpl.rc('hatch', color='k', linewidth=0.5)
    # Add some text for labels, title and custom x-axis tick labels, etc.
    ax.set_ylabel('Flow Completion Time [s]', fontsize=13)
    # ax.set_title('Link Capacity: 10Gbps', fontsize=16)
    ax.set_xticks(x)
    # ax.set_yticks(np.arange(0, 12.1, 3))
    ax.set_yticks(np.array([0, 3, 6, 9, 11]))
    ax.set_xticklabels(labels, fontsize=13)
    plt.yticks(fontsize=12)
    legend = ax.legend(fontsize=14, loc='upper left')
    ax.grid(True, which='major', axis='y', lw=0.65, ls='--', dashes=(3, 7), zorder=0)

    autolabel(ax, rects1)
    autolabel(ax, rects2)
    fig.set_size_inches(5, 3.75)
    plt.draw() # Draw the figure so you can find the positon of the legend.

    # plt.margins(x=0.02)
    plt.tight_layout()
    # Get the bounding box of the original legend
    bb = legend.get_bbox_to_anchor().inverse_transformed(ax.transAxes)

    # Change to location of the legend.
    xOffset = 0.015
    yOffset = 0.015
    bb.x0 += xOffset
    bb.x1 += xOffset
    bb.y0 += yOffset
    bb.y1 += yOffset
    legend.set_bbox_to_anchor(bb, transform=ax.transAxes)
    legend.get_frame().set_linewidth(0.5)
    plt.savefig(savefig_path + '10gbps.eps',
                format='eps', dpi=1200, bbox_inches='tight', pad_inches=0.01)

if __name__ == '__main__':
    # run_1g()
    run_10g()
