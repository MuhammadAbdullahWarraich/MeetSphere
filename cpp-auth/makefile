all: login_client signup_client server

signup_client: signup_client.cpp
	g++ -o signup_client signup_client.cpp -lssl -lcrypto
login_client: login_client.cpp
	g++ -o login_client login_client.cpp -lssl -lcrypto
server: server.cpp
	g++ -o server server.cpp
