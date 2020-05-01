all:
	g++ client.cpp -Wall -pthread -g -o client
	g++ server.cpp -Wall -pthread -g -o server

rm:
	rm client
	rm server

runClient:
	./client localhost 52547

runServer:
	./server 52547
