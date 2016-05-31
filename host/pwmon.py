import sys, serial
from serial.tools.list_ports import comports

valid_controllers   = ['USB VID:PID=0403:6001', 'FTDIBUS\\VID_0403+PID_6001', 'USB VID:PID=10C4:EA60']
controller_baudrate = 115200
controller_timeout  = 1
controller_channels = 8

def find_port():
	for port, info, descr in comports():
		for c in valid_controllers:
			if descr.startswith(c):
				return port
	return None

def open_port(port):
	return serial.Serial(port, timeout = controller_timeout, baudrate = controller_baudrate)

def read_stat(com):
	com.write(b's\r')
	prefix = com.read(6)
	if len(prefix) == 0:
		raise RuntimeError('failed to read response')
	if len(prefix) != 6 or prefix[0:1] != b'~':
		raise RuntimeError('invalid prefix: %s' % prefix)
	sz = int(prefix[1:5], base=16)
	resp = com.read(sz)
	if len(resp) != sz:
		raise RuntimeError('invalid response: %s' % resp)
	return resp.replace('\r', '\n')

def main():
	port = find_port()
	if not port:
		print 'receiver not found'
		return -1
	com = open_port(port)
	print read_stat(com)
	return 0

if __name__ == '__main__':
	sys.exit(main())
