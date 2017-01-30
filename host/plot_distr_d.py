import sys
import matplotlib
import matplotlib.pyplot as plt
from plot_common import *

file, time_range, other = parse_pw_opts()
T, D, Y = load(file, time_range)

days = [int(o) for o in other]
if not days:
	days = range(7)

for d in days:
	X, W = get_pw_distr(select_weekday(D, Y, d))
	plt.plot(X, W, color=week_day_colors[d], label=week_day_names[d])
plt.legend()
plt.show()


