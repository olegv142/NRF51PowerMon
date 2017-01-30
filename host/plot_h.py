import sys
import matplotlib
import matplotlib.pyplot as plt
from plot_common import *

file, time_range, _ = parse_pw_history_opts()
T, D, Y = load(file, time_range)

W = get_hist_h(D, Y)

plt.bar(range(24), W)
plt.xlim(0, 24)
plt.show()

