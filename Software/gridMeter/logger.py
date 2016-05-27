"""Usage:
	logger.py <port> [--timeout=seconds]

"""

from docopt import docopt
import serial
import datetime

def start_logging(port):
	with serial.Serial(port, 9600) as ser:
		while(True):
			line = ser.readline()
			print("{}, {}".format(datetime.datetime.now(), line.decode("utf-8").strip()))

if __name__ == "__main__":
	args = docopt(__doc__)
	start_logging(args["<port>"])
