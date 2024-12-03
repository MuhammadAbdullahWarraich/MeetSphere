#! the retval depends upon the answer to the following question:
        # Are we making a new TCP connection on client side for each of these requests or are we reusing the same old socket used to send the first query from the client to the server?

import socket
import sqlite3
import threading
import re

def handle_signup_req(req, client_sock, db):
    pos = req.find('username=') + 9
    username = req[pos : req.find('&', pos)]

    pos = req.find('password=') + 9
    password = req[pos:]

    cursor = db.cursor()
    try:
        cursor.execute("INSERT INTO Users (username, password) VALUES (?, ?)", (username, password))
        db.commit()
        response = "Signup successful\n"
        print(f"User signed up: {username}")
    except sqlite3.IntegrityError:
        response = "Error: Username already exists\n"
    client_sock.send(response.encode('utf-8'))
    
    return False #!

def handle_login_req(req, client_sock, db):
    pos = req.find('username=') + 9
    username = req[pos : req.find('&', pos)]

    pos = req.find('password=') + 9
    password = req[pos:]

    cursor = db.cursor()

    cursor.execute("SELECT password FROM Users WHERE username = ?", (username,))
    row = cursor.fetchone()
    
    if row and row[0] == password:
        response = "Login successful\n"
        
        # Check if user is already logged in by checking LoggedInUsers table
        cursor.execute("SELECT 1 FROM LoggedInUsers WHERE username = ?", (username,))
        if cursor.fetchone():
            response = "You are already logged in on another device"
            print(f"User login error: second device ; username = {username}")
        else:
            # Add user to LoggedInUsers table to mark as logged in
            cursor.execute("INSERT INTO LoggedInUsers (username) VALUES (?)", (username,))
            db.commit()
            print(f"User logged in: {username}")
    else:
        response = "Error: Invalid username or password\n"
    
    client_sock.send(response.encode('utf-8'))
    return False  #!


def handle_logout_req(req, client_sock, db):
    pos = req.find('username=') + 9
    username = req[pos : req.find('&', pos)]

    cursor = db.cursor()

    # Remove the user from the LoggedInUsers table if they are logged in
    cursor.execute("DELETE FROM LoggedInUsers WHERE username = ?", (username,))
    db.commit()

    if cursor.rowcount > 0:
        print(f"User logged out ; username = {username}")
        response = "Logout successful\n"
    else:
        response = "Error: User not logged in\n"
    
    client_sock.send(response.encode('utf-8'))
    return False  #!

req_patterns = {
    re.compile(r'^action=signup&username=\w+&password=.+$') : handle_signup_req,
    re.compile(r'^action=login&username=\w+&password=.+$') : handle_login_req,
    re.compile(r'^action=logout&username=\w+$') : handle_logout_req,
  #  re.compile(r'^$') : handle_create_meeting_req,
   # re.compile(r'^$') : handle_join_meeting_req
}

def initialize_database():
    try:
        db = sqlite3.connect("meetsphere_database.db")
        cursor = db.cursor()

        # Create Users table if it doesn't already exist
        cursor.execute('''
        CREATE TABLE IF NOT EXISTS Users (
            username TEXT UNIQUE NOT NULL,
            password TEXT NOT NULL
        )
        ''')

        # Create Meetings table if it doesn't already exist
        cursor.execute('''
        CREATE TABLE IF NOT EXISTS Meetings (
            mid INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL
        )
        ''')

        # Create Participants table if it doesn't already exist
        cursor.execute('''
        CREATE TABLE IF NOT EXISTS Participants (
            mid INTEGER NOT NULL,
            username TEXT NOT NULL,
            userIp TEXT NOT NULL,
            userPort TEXT NOT NULL
        )
        ''')

        # Create LoggedInUsers table to track logged in users
        cursor.execute('''
        CREATE TABLE IF NOT EXISTS LoggedInUsers (
            username TEXT UNIQUE NOT NULL
        )
        ''')

        db.commit()
        return db

    except sqlite3.Error as e:
        print(f"Error opening SQLite database: {e}")
        return None

def handle_client(client_sock, client_addr):
    db = sqlite3.connect("meetsphere_database.db")
    loop_flag = True
    while loop_flag:

        request = client_sock.recv(1024).decode('utf-8')

        if not request:
            print("Failed to receive data or connection closed")
            client_sock.close()
            return

        print(f"Received request: {request}")
        nomatch = True

        for regex, func in req_patterns.items():
            if regex.match(request):
                loop_flag = func(request, client_sock, db)
                nomatch = False
                break
        
        if nomatch == True:
            error_response = "Invalid request format\n"
            client_sock.send(error_response.encode('utf-8'))
            client_sock.close()
            return

    client_sock.close()


def start_server(server_ip, server_port):

    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    server_sock.bind((server_ip, server_port))
    server_sock.listen(5)

    print(f"Server is listening on {server_ip}:{server_port}")

    while True:
        client_sock, client_addr = server_sock.accept()
        print(f"Client connected: {client_addr}")
        client_thread = threading.Thread(target=handle_client, args=(client_sock, client_addr))
        client_thread.start()


if __name__ == "__main__":
    server_ip = "127.0.0.1"
    server_port = 8080

    db = initialize_database()
    if db:
        start_server(server_ip, server_port)