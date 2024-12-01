#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/sha.h> // For password hashing
#include <unistd.h>      // For close()
bool signUp(const std::string& username, const std::string& password, const std::string& server_ip, int server_port) {
    // Step 1: Hash the password using SHA-256
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(password.c_str()), password.size(), hash);

    // Convert the hash to a hexadecimal string
    std::string encrypted_password;
    char buf[3];
    for (unsigned char c : hash) {
        snprintf(buf, sizeof(buf), "%02x", c);
        encrypted_password += buf;
    }

    // Step 2: Create the TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "Failed to create socket\n";
        return false;
    }

    // Step 3: Configure the server address
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip.c_str(), &server_address.sin_addr) <= 0) {
        std::cerr << "Invalid server IP address\n";
        close(sock);
        return false;
    }

    // Step 4: Connect to the server
    if (connect(sock, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) < 0) {
        std::cerr << "Connection to server failed\n";
        close(sock);
        return false;
    }

    // Step 5: Prepare the signup message
    std::string message = "action=signup&username=" + username + "&password=" + encrypted_password;

    // Step 6: Send the signup request
    if (send(sock, message.c_str(), message.size(), 0) == -1) {
        std::cerr << "Failed to send data to server\n";
        close(sock);
        return false;
    }

    std::cout << "Signup request sent successfully\n";

    // Step 7: Receive response from the server
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        std::cerr << "Failed to receive response from server\n";
        close(sock);
        return false;
    }

    buffer[bytes_received] = '\0'; // Null-terminate the string
    std::cout << "Server Response: " << buffer << "\n";

    // Step 8: Close the socket
    close(sock);
    return true;
}
int main() {
    std::string username;
    std::string password;
    std::string server_ip = "127.0.0.1";
    int server_port = 8080;

    std::cout << "Enter username: ";
    std::cin >> username;
    std::cout << "Enter password: ";
    std::cin >> password;

    if (signUp(username, password, server_ip, server_port)) {
        std::cout << "Signup process completed.\n";
    } else {
        std::cerr << "Signup process failed.\n";
    }

    return 0;
}
