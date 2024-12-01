#include <iostream>
#include <string>
#include <cstring>
#include <unordered_map>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>

// In-memory database and mutex for thread safety
std::unordered_map<std::string, std::string> userDatabase;
std::mutex dbMutex; // Ensures thread-safe access to the database

void handleClient(int client_sock) {
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    // Receive data from the client
    int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        std::cerr << "Failed to receive data or connection closed\n";
        close(client_sock);
        return;
    }

    buffer[bytes_received] = '\0'; // Null-terminate the string
    std::string request(buffer);
    std::cout << "Received request: " << request << "\n";

    // Parse the action, username, and password
    size_t action_pos = request.find("action=");
    size_t username_pos = request.find("username=");
    size_t password_pos = request.find("password=");

    if (action_pos == std::string::npos || username_pos == std::string::npos || password_pos == std::string::npos) {
        std::string error_response = "Invalid request format\n";
        send(client_sock, error_response.c_str(), error_response.size(), 0);
        close(client_sock);
        return;
    }

    std::string action = request.substr(action_pos + 7, username_pos - (action_pos + 8));
    std::string username = request.substr(username_pos + 9, password_pos - (username_pos + 10));
    std::string password = request.substr(password_pos + 9);

    if (action == "signup") {
        // Lock the database for thread-safe access
        std::lock_guard<std::mutex> lock(dbMutex);

        // Check if the username already exists
        if (userDatabase.find(username) != userDatabase.end()) {
            std::string response = "Error: Username already exists\n";
            send(client_sock, response.c_str(), response.size(), 0);
        } else {
            // Store the username and hashed password
            userDatabase[username] = password;
            std::string response = "Signup successful\n";
            send(client_sock, response.c_str(), response.size(), 0);
            std::cout << "User signed up: " << username << "\n";
        }
    } else if (action == "login") {
        // Lock the database for thread-safe access
        std::lock_guard<std::mutex> lock(dbMutex);

        // Validate the username and password
        auto it = userDatabase.find(username);
        if (it != userDatabase.end() && it->second == password) {
            std::string response = "Login successful\n";
            send(client_sock, response.c_str(), response.size(), 0);
            std::cout << "User logged in: " << username << "\n";
        } else {
            std::string response = "Error: Invalid username or password\n";
            send(client_sock, response.c_str(), response.size(), 0);
        }
    } else {
        std::string error_response = "Unknown action\n";
        send(client_sock, error_response.c_str(), error_response.size(), 0);
    }

    // Close the client socket
    close(client_sock);
}

void startServer(const std::string& server_ip, int server_port) {
    // Step 1: Create the socket
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        std::cerr << "Failed to create server socket\n";
        return;
    }

    // Step 2: Bind the socket to the IP and port
    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    server_address.sin_addr.s_addr = inet_addr(server_ip.c_str());

    if (bind(server_sock, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) < 0) {
        std::cerr << "Binding failed\n";
        close(server_sock);
        return;
    }

    // Step 3: Listen for incoming connections
    if (listen(server_sock, 5) < 0) {
        std::cerr << "Listening failed\n";
        close(server_sock);
        return;
    }

    std::cout << "Server is listening on " << server_ip << ":" << server_port << "\n";

    // Step 4: Accept and handle client connections
    while (true) {
        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);
        int client_sock = accept(server_sock, reinterpret_cast<sockaddr*>(&client_address), &client_len);
        if (client_sock < 0) {
            std::cerr << "Failed to accept connection\n";
            continue;
        }

        std::cout << "Client connected\n";

        // Launch a new thread to handle the client
        std::thread clientThread(handleClient, client_sock);
        clientThread.detach(); // Detach the thread to allow independent execution
    }

    // Step 5: Close the server socket (unreachable in this example)
    close(server_sock);
    std::cout << "Server shut down\n";
}
int main() {
    std::string server_ip = "127.0.0.1";
    int server_port = 8080;

    startServer(server_ip, server_port);

    return 0;
}
