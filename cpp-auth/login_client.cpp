#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/sha.h>
void sendRequest(const std::string& server_ip, int server_port, const std::string& username, const std::string& password) {
    // Create the socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "Failed to create socket\n";
        return;
    }

    // Server address
    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip.c_str(), &server_address.sin_addr);

    // Connect to the server
    if (connect(sock, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) < 0) {
        std::cerr << "Failed to connect to server\n";
        close(sock);
        return;
    }
    // Hash the password using SHA-256
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(password.c_str()), password.size(), hash);

    // Convert the hash to a hexadecimal string
    std::string encrypted_password;
    char buf[3];
    for (unsigned char c : hash) {
        snprintf(buf, sizeof(buf), "%02x", c);
        encrypted_password += buf;
    }
    // Prepare the login request
    std::string request = "action=login&username=" + username + "&password=" + encrypted_password;

    // Send the request
    send(sock, request.c_str(), request.size(), 0);

    // Receive the response
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        std::cout << "Server response: " << buffer << "\n";
    }

    // Close the socket
    close(sock);
}

int main() {
    std::string server_ip = "127.0.0.1";
    int server_port = 8080;

    // Test login
    std::string username, password;
    std::cout << "Enter username: ";
    std::cin >> username;
    std::cout << "Enter password: ";
    std::cin >> password;

    sendRequest(server_ip, server_port, username, password);

    return 0;
}
