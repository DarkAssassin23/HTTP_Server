#!/usr/bin/python3

#
# Basic client to test connecting to the server
#

import socket, ipaddress, sys, time, random

# Default host and port to connect to
HOST = "localhost"
PORT = 4080
HTTP_END = "HTTP/1.1\r\n\r\n"

# Invalid http version test
def invalidHTTP():
	return "GET / HTTP/0.9\r\n\r\n"

# Send garbage
def garbageHTTP():
	return f"'%s'" % ("A" * 50000)

# Try to overflow
def overflow():
	return f"GET /%s {HTTP_END}" % ("A" * 4080)

def attack():
	return f"GET /rm -rf /* {HTTP_END}"

def absoluteURL():
	return f"GET https://google.com {HTTP_END}"

# Bad format
def badFormat():
	return f"GET {HTTP_END}"

def validRequest(method, file):
	return f"{method} /{file} {HTTP_END}"

def emptyRequest():
	return ""

# Checks if IP Address passed in is valid
def validIP(ip):
	if(ip=="localhost"):
		return True
	bits = ip.split('.')
	if(len(bits)>4 or len(bits)<4):
		return False
	for x in bits:
		if(x.isdigit()):
			if(int(x)>255):
				return False
			elif(not x.isdigit()):
				return False
	return True

# Checks if the Port number passed in is Valid
def validPort(port):
	if(not port.isdigit()):
		return False
	if(int(port)<0 or int(port)>65535):
		return False
	return True

if(len(sys.argv)>2 and validPort(sys.argv[2])):
    PORT = (int)(sys.argv[2])
if(len(sys.argv)>1 and validIP(sys.argv[1])):
    HOST = sys.argv[1]
	
filesToRequest = ["makefile", "src/server.c", "src/queue.c", "headers/queue.h",
				  "", "invalid.c", "test/example.json"]
# Create socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Time took to recieve data
start = time.time()

# Connect to socket
sock.connect((HOST, PORT))

# Create and send message
fileToRequest = random.randint(0,len(filesToRequest)-1)
rand = 0
og = ""
data = validRequest("GET", filesToRequest[fileToRequest])
if(random.randint(0, 10) >= 5):
	rand = random.randint(0,10)
	if(rand == 0):
		data = invalidHTTP()
	elif(rand == 1):
		data = garbageHTTP()
		og = "Garbage Data"
	elif(rand == 2):
		data = badFormat()
	elif(rand == 3):
		data = validRequest("OPTIONS", "")
	elif(rand == 4):
		data = validRequest("DELETE", filesToRequest[fileToRequest])
	elif(rand == 5):
		data = emptyRequest()
		og = "emptyRequest"
	elif(rand == 6):
		data = overflow()
		og = "overflow attempt"
	elif(rand == 7):
		data = absoluteURL()
	elif (rand == 8):
		data = attack()
	else:
		data = validRequest("HEAD", filesToRequest[fileToRequest])

# print(data)
if(og == ""):
	og = data.strip()
data = data.encode()
sock.sendall(data)

# Recieve and decode response
data = sock.recv(4096)
msg = data.decode()

# Print out response
# print(msg)

# Close connection
sock.close()

end = time.time() - start

print(f"Time Elapsed: {end}, Sent request {og}, resp:")
if(msg == ""):
	print("no")
else:
	print(f"{msg}\n\n")
