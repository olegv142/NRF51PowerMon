import sys
import matplotlib
import matplotlib.pyplot as plt
from plot_common import *

file, time_range, _ = parse_pw_opts()
T, D, Y = load(file, time_range)

X, D = get_pw_distr(Y)

plt.plot(X, D)
plt.show()

