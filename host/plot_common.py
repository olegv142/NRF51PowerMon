import sys
import time
from datetime import datetime

week_day_names = ['mon', 'tue', 'wed', 'thu', 'fri', 'sat', 'sun']
week_day_colors = ['y', 'g', 'b', 'c', 'olive', 'm', 'r']

def load(file, time_range = None):
	now = time.time()
	tmin = now - time_range if time_range is not None else 0.
	T, D, Y = [], [], []
	with open(file, 'r') as f:
		for line in f.readlines():
			words = line.split()
			if len(words) == 2:
				t = int(words[0])
				y = float(words[1])
				if t > tmin: 
					d = datetime.fromtimestamp(t)
					T.append(t)
					D.append(d)
					Y.append(y)
	return T, D, Y

def parse_pw_history_opts():
	file = 'pw_history.dat'
	time_range = None
	other_opts = []
	for opt in sys.argv[1:]:
		if opt.startswith('-'):
			suff = opt[-1]
			if suff == 'm':
				mult = 3600 * 24 * 30
			elif suff == 'w':
				mult = 3600 * 24 * 7
			elif suff == 'd':
				mult = 3600 * 24
			else:
				other_opts.append(opt[1:])
				continue
			time_range = int(opt[1:-1]) * mult
		else:
			file = opt
	return file, time_range, other_opts

def parse_pw_opts():
	file = 'pw.dat'
	time_range = None
	other_opts = []
	for opt in sys.argv[1:]:
		if opt.startswith('-'):
			suff = opt[-1]
			if suff == 'm':
				mult = 3600 * 24 * 30
			elif suff == 'w':
				mult = 3600 * 24 * 7
			elif suff == 'd':
				mult = 3600 * 24
			elif suff == 'h':
				mult = 3600
			else:
				other_opts.append(opt[1:])
				continue
			time_range = int(opt[1:-1]) * mult
		else:
			file = opt
	return file, time_range, other_opts

def get_hist_h(D, Y):
	Tot, Cnt = [0.] * 24, [0] * 24
	for d, y in zip(D, Y):
		Cnt[d.hour] += 1
		Tot[d.hour] += y
	return [Tot[i] / Cnt[i] for i in range(24)]

def get_hist_w(D, Y):
	Tot, Cnt = [0.] * 7, [0] * 7
	for d, y in zip(D, Y):
		w = d.weekday()
		Cnt[w] += 1
		Tot[w] += y
	return [Tot[i] / Cnt[i] for i in range(7)]

def get_hist_hw(D, Y):
	Tot, Cnt = [[0.] * 24 for day in range(7)], [[0] * 24 for day in range(7)]
	for d, y in zip(D, Y):
		w = d.weekday()
		Cnt[w][d.hour] += 1
		Tot[w][d.hour] += y
	return [[Tot[day][i] / Cnt[day][i] for i in range(24)] for day in range(7)]

def get_pw_distr(Y, ymax=4000., bins=1000):
	Btot, cnt = [0.] * bins, 0
	step = ymax / bins
	for y in Y:
		bin = min(int(y / step), bins-1)
		Btot[bin] += y
		cnt += 1
	Tot = sum(Btot)
	B = [step * i for i in range(bins + 1)]
	D = [0.] * (bins + 1)
	if not cnt:
		return B, D
	D[0] = Tot/cnt
	for i in range(bins):
		Tot -= Btot[i]
		D[i+1] = max(Tot/cnt, 0.)
	return B, D

def select_weekday(D, Y, wd):
	 return [y for d, y in zip(D, Y) if d.weekday() == wd]
