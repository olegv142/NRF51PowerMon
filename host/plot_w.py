import sys
import matplotlib
import matplotlib.pyplot as plt
from plot_common import *

file, time_range, _ = parse_pw_history_opts()
T, D, Y = load(file, time_range)

W = get_hist_w(D, Y)

plt.bar(range(5), W[:5], color='y')
plt.bar(range(5,7), W[5:7], color='m')
plt.xlim(0, 7)
plt.show()

