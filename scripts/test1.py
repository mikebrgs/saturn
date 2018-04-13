import sys, socket, thread, time, threading

SIZE = 128

class ClientLife(threading.Thread):
	def __init__(self, address, port, service):
		threading.Thread.__init__(self)
		self.central_server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		self.address = address
		self.port = port
		self.service = service
		self.central_server.sendto(("GET_DS_SERVER " + self.service).encode(),
			(self.address, int(self.port)))
		data, addr = self.central_server.recvfrom(SIZE)
		token_buffer = data.decode()
		print("received message:" + token_buffer + "\n")
		if token_buffer == "OK 0;0.0.0.0;0":
			print("error with cs")
			return
		split1 = token_buffer.split(" ")
		split2 = split1[1].split(";")
		self.server_address = split2[1]
		self.server_port = int(split2[2])
		self.server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		self.server.sendto(("MY_SERVICE ON").encode(),
			(self.server_address, self.server_port))
		data, addr = self.server.recvfrom(SIZE)
		token_buffer = data.decode()
		print("received message:" + token_buffer + "\n")

	def run(self):
		self.server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		self.server.sendto(("MY_SERVICE OFF").encode(),
			(self.server_address, self.server_port))
		data, addr = self.server.recvfrom(SIZE)
		token_buffer = data.decode()
		print("received message:" + token_buffer + "\n")

if sys.argv.__len__() != 4:
	print("python <path>/test1.py <address> <port> <service>")
	sys.exit(1)

client1 = ClientLife(sys.argv[1], sys.argv[2], sys.argv[3])
time.sleep(2)
client2 = ClientLife(sys.argv[1], sys.argv[2], sys.argv[3])
time.sleep(2)
client3 = ClientLife(sys.argv[1], sys.argv[2], sys.argv[3])
time.sleep(2)
client4 = ClientLife(sys.argv[1], sys.argv[2], sys.argv[3])
time.sleep(2)
client5 = ClientLife(sys.argv[1], sys.argv[2], sys.argv[3])
time.sleep(2)
client6 = ClientLife(sys.argv[1], sys.argv[2], sys.argv[3])
time.sleep(2)
client7 = ClientLife(sys.argv[1], sys.argv[2], sys.argv[3])
time.sleep(2)
client8 = ClientLife(sys.argv[1], sys.argv[2], sys.argv[3])
time.sleep(2)
client9 = ClientLife(sys.argv[1], sys.argv[2], sys.argv[3])
time.sleep(2)
client10 = ClientLife(sys.argv[1], sys.argv[2], sys.argv[3])
time.sleep(2)
client1.start()
client2.start()
client3.start()
client4.start()
client5.start()
client6.start()
client7.start()
client8.start()
client9.start()
client10.start()
client1.join()
client2.join()
client3.join()
client4.join()
client5.join()
client6.join()
client7.join()
client8.join()
client9.join()
client10.join()
