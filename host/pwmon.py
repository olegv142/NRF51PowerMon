import sys, serial
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

# data page size
page_sz = 1024

# Transfer status
x_none         = 0
x_starting     = 1
x_reading_meta = 2
x_reading_data = 3
x_completed    = 4
x_failed       = 5

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

def transfer_data(com, status_cb=None):
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

#----------------------------------------------------------

def get_pages(com):
	status_text = {
		x_starting     : 'starting',
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

	pages = transfer_data(com, status_cb)
	for pg in pages:
		print bin2hex(pg)

def main():
	port = find_port()
	if not port:
		print 'receiver not found'
		return -1

	com = open_port(port)
	if len(sys.argv) <= 1:
		print send_text_command(com, 'r')
		return 0

	if '--get-pages' in sys.argv[1:]:
		get_pages(com)
		return 0

	for arg in sys.argv[1:]:
		if arg[0] != '-':
			print send_text_command(com, arg)

	return 0

if __name__ == '__main__':
	sys.exit(main())
