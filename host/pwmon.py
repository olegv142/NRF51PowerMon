import sys, serial, time, struct
from serial.tools.list_ports import comports
from collections import namedtuple

valid_controllers   = ['USB VID:PID=0403:6001', 'FTDIBUS\\VID_0403+PID_6001', 'USB VID:PID=10C4:EA60']
controller_baudrate = 230400
controller_timeout  = 1

#----------- Core communication functions --------------------

def find_port():
	for port, info, descr in comports():
		for c in valid_controllers:
			if descr.startswith(c):
				return port
	return None

def open_port(port):
	return serial.Serial(port, timeout = controller_timeout, baudrate = controller_baudrate)

def bin2hex(resp):
	return ' '.join(['%02x' % ord(c) for c in resp])

def read_response(com, printable=False):
	prefix = com.read(6)
	if len(prefix) == 0:
		raise RuntimeError('failed to read response')
	if len(prefix) != 6 or prefix[0:1] != '~':
		raise RuntimeError('invalid prefix: %s' % prefix)
	sz = int(prefix[1:5], base=16)
	resp = com.read(sz)
	if len(resp) != sz:
		raise RuntimeError('invalid response: %s' % resp)
	if not printable:
		return resp
	if prefix[5] == 'B':
		return bin2hex(resp)
	else:
		return resp.replace('\r', '\n')

def send_command(com, cmd):
	com.write(cmd + '\r')
	return read_response(com)

def send_text_command(com, cmd):
	com.write(cmd + '\r')
	return read_response(com, printable=True)

#------- Data reading ------------------------------------

# Transfer status
x_none         = 0
x_starting     = 1
x_reading_meta = 2
x_reading_data = 3
x_completed    = 4
x_failed       = 5

# Data domains
d_pw         = 0
d_pw_history = 1
d_vbatt      = 2

# Data scaling
scale_pw    = .2
scale_vbatt = .0001

# Scales per domain
d_scale = (scale_pw, scale_pw, scale_vbatt)

# Measuring period
measuring_period = 12

# Measuring period per domain
d_measuring_period = (measuring_period, 3600, 600)

DataPage = namedtuple('DataPage', ('domain', 'page_idx', 'sn', 'data'))

page_sz           = 1024
page_frag_sz      = page_sz // 8
page_hdr_fmt      = 'BBBBI'
page_hdr_sz       = struct.calcsize(page_hdr_fmt)
page_item_fmt     = 'H'
page_item_sz      = struct.calcsize(page_item_fmt)
page_items        = (page_sz - page_hdr_sz) // page_item_sz
page_frag_items   = page_frag_sz // page_item_sz
page_hdr_items    = page_hdr_sz  // page_item_sz
page_item_invalid = 0xffff

def parse_data_page(d):
	hdr          = struct.unpack(page_hdr_fmt, d[:page_hdr_sz])
	items        = struct.unpack(page_item_fmt * page_items, d[page_hdr_sz:])
	unused_frags = hdr[2]
	return DataPage(
				domain   = hdr[0],
				page_idx = hdr[1],
				sn       = hdr[-1],
				data     = [
							it for j, it in enumerate(items) if
								not ((1 << ((page_hdr_items + j) // page_frag_items)) & unused_frags) and
								it != page_item_invalid
						]
			)

def get_transmitter_uptime(com):
	return int(send_command(com, 'u'))

def get_transmitter_start_time(com):
	return int(time.time()) - get_transmitter_uptime(com)

def start_transfer(com):
	r = send_command(com, 's')
	if r != '\r':
		raise RuntimeError('invalid response: %s' % r)

def query_data_page(com):
	r = send_command(com, 'qd')
	if len(r) == 1:
		return ord(r[0]), None
	if len(r) != 1 + page_sz:
		raise RuntimeError('invalid data length: %u bytes' % len(r))
	return ord(r[0]), r[1:]

def retrieve_data_raw(com, status_cb=None):
	start_transfer(com)
	pages = []
	while True:
		sta, data = query_data_page(com)
		if status_cb is not None:
			status_cb(sta)
		if data is not None:
			pages.append(data)
		else:
			if sta == x_failed:
				raise RuntimeError('data transfer failed')
			if sta == x_completed:
				return pages

def retrieve_data_pages(com, status_cb=None):
	raw_pages = retrieve_data_raw(com, status_cb)
	return [parse_data_page(p) for p in raw_pages]

def retrieve_data(com, status_cb=None):
	d_pages = {d_pw : [], d_pw_history : [], d_vbatt : []}
	d_data  = {d_pw : [], d_pw_history : [], d_vbatt : []}
	ts = get_transmitter_start_time(com)
	pages = retrieve_data_pages(com, status_cb)
	for p in pages:
		d_pages[p.domain].append(p)
	for d, pgs in d_pages.items():
		data, scale, period = d_data[d], d_scale[d], d_measuring_period[d]
		pgs.sort(key = lambda p: p.sn)
		for p in pgs:
			toff  = ts + p.sn * measuring_period
			for i, v in enumerate(p.data):
				data.append((toff + i * period, v * scale))
	return d_data

#----------------------------------------------------------

def get_status_cb():
	status_text = {
		x_starting     : 'connecting',
		x_reading_meta : 'reading metadata',
		x_reading_data : 'reading data',
		x_completed    : 'completed',
		x_failed       : 'failed'
	}
	ctx = [x_none]
	def status_cb(sta):
		if ctx[0] != sta:
			ctx[0] = sta
			print >> sys.stderr, status_text[sta]

	return status_cb

def get_raw_pages(com):
	pages = retrieve_data_raw(com, get_status_cb())
	for pg in pages:
		print bin2hex(pg)

def get_pages(com):
	pages = retrieve_data_pages(com, get_status_cb())
	for pg in pages:
		print pg

def save_data(com, names):
	data = retrieve_data(com, get_status_cb())
	for d, items in data.items():
		with open(names[d], 'w') as f:
			for t, v in items:
				print >> f, t, v

def main():
	port = find_port()
	if not port:
		print 'receiver not found'
		return -1

	com = open_port(port)
	if len(sys.argv) <= 1:
		print send_text_command(com, 'r')
		return 0

	args = sys.argv[1:]

	if '--get-raw-pages' in args:
		get_raw_pages(com)
		return 0

	if '--get-pages' in args:
		get_pages(com)
		return 0

	if '--save-data' in args:
		if len(args) == 4:
			args.remove('--save-data')
			save_data(com, args)
		else:
			save_data(com, ('pw.dat', 'pw_history.dat', 'vbatt.dat'))
		return 0

	for arg in args:
		if arg[0] != '-':
			print send_text_command(com, arg)

	return 0

if __name__ == '__main__':
	sys.exit(main())
