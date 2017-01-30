import sys
import matplotlib
import matplotlib.pyplot as plt
from plot_common import *

file, time_range, other = parse_pw_history_opts()
T, D, Y = load(file, time_range)

W = get_hist_hw(D, Y)

days = [int(o) for o in other]
if not days:
	days = range(7)

w = .5/len(days)
for i, d in enumerate(days):
	plt.bar([h + i*1.6*w for h in range(24)], W[d], width=w, color=week_day_colors[d], linewidth=0, label=week_day_names[d])

plt.xlim(0, 24)
plt.legend(loc='upper left')
plt.show()

