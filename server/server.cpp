#include <iostream>
#include <string>
#include <unordered_map>
#include <map>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <sqlite3.h>

// Function to initialize the SQLite database
sqlite3* initializeDatabase() {
    sqlite3* db;
    int rc = sqlite3_open("users.db", &db);

    if (rc) {
        std::cerr << "Error opening SQLite database: " << sqlite3_errmsg(db) << "\n";
        return nullptr;
    }

    // Create a table for storing users if it doesn't already exist
    const char* createUsersTableSQL = R"(
        CREATE TABLE IF NOT EXISTS Users (
            username TEXT UNIQUE NOT NULL,
            password TEXT NOT NULL
        );
    )";

    char* errMsg = nullptr;
    rc = sqlite3_exec(db, createUsersTableSQL, nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "Error creating table: " << errMsg << "\n";
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return nullptr;
    }

    // Create a table for storing meetings if it doesn't already exist
    const char* createMeetingsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS Meetings (
            mid INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL
        );
    )";
    
    errMsg = nullptr;
    rc = sqlite3_exec(db, createMeetingsTableSQL, nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        std::cerr << "Error creating table: " << errMsg << "\n";
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return nullptr;
    }

    // Create a table for storing meeting participants if it doesn't already exist
    const char* createParticipantsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS Participants (
            mid INTEGER NOT NULL,
            username TEXT NOT NULL,
            userIp TEXT NOT NULL,
            userPort TEXT NOT NULL,
        );
    )";

    errMsg = nullptr;
    rc = sqlite3_exec(db, createParticipantsTableSQL, nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        std::cerr << "Error creating table: " << errMsg << "\n";
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return nullptr;
    }
    
    // return
    return db;
}

// Function to handle client requests
void handleClient(int client_sock, sqlite3* db, std::unordered_map<std::string, bool>* loginned) {
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
        // Insert the username and password into the SQLite database
        const char* insertSQL = "INSERT INTO Users (username, password) VALUES (?, ?)";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr);

        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << "\n";
            close(client_sock);
            return;
        }

        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE) {
            std::string response = "Signup successful\n";
            send(client_sock, response.c_str(), response.size(), 0);
            std::cout << "User signed up: " << username << "\n";
        } else {
            std::string response = "Error: Username already exists\n";
            send(client_sock, response.c_str(), response.size(), 0);
        }

        sqlite3_finalize(stmt);
    } else if (action == "login") {
        // Check the username and password in the SQLite database
        const char* querySQL = "SELECT password FROM Users WHERE username = ?";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, querySQL, -1, &stmt, nullptr);

        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << "\n";
            close(client_sock);
            return;
        }

        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const char* storedPassword = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (password == storedPassword) {
                std::string response = "Login successful\n";
                if (loginned->find(username) != loginned->end()) {
                    response = "You are already logged in on another device";
                    std::cout << "User login error: second device ; username = " << username << "\n";
                } else {
                    std::cout << "User logged in: " << username << "\n";
                    loginned->insert({username, true});
                }
                send(client_sock, response.c_str(), response.size(), 0);
                
            } else {
                std::string response = "Error: Invalid username or password\n";
                send(client_sock, response.c_str(), response.size(), 0);
            }
        } else {
            std::string response = "Error: Invalid username or password\n";
            send(client_sock, response.c_str(), response.size(), 0);
        }

        sqlite3_finalize(stmt);
    } else if (action == "logout") {
        loginned->erase(username);
        std::cout << "User logged out ; username = " << username << "\n";
        std::string response = "Logout successful\n";
        send(client_sock, response.c_str(), response.size(), 0);
    } else {
        std::string error_response = "Unknown action\n";
        send(client_sock, error_response.c_str(), error_response.size(), 0);
    }

    // Close the client socket
    close(client_sock);
}

void startServer(const std::string& server_ip, int server_port, sqlite3* db) {
    std::unordered_map<std::string, bool> loginned;
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        std::cerr << "Failed to create server socket\n";
        return;
    }

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    server_address.sin_addr.s_addr = inet_addr(server_ip.c_str());

    if (bind(server_sock, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) < 0) {
        std::cerr << "Binding failed\n";
        close(server_sock);
        return;
    }

    if (listen(server_sock, 5) < 0) {
        std::cerr << "Listening failed\n";
        close(server_sock);
        return;
    }

    std::cout << "Server is listening on " << server_ip << ":" << server_port << "\n";

    while (true) {
        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);
        int client_sock = accept(server_sock, reinterpret_cast<sockaddr*>(&client_address), &client_len);
        if (client_sock < 0) {
            std::cerr << "Failed to accept connection\n";
            continue;
        }

        std::cout << "Client connected\n";
        std::thread clientThread(handleClient, client_sock, db, &loginned);
        clientThread.detach();
    }

    close(server_sock);
}

int main() {
    std::string server_ip = "127.0.0.1";
    int server_port = 8080;

    sqlite3* db = initializeDatabase();
    if (!db) {
        return -1;
    }

    startServer(server_ip, server_port, db);

    sqlite3_close(db);
    return 0;
}
