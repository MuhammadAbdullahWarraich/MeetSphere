#----------------------------------------------------------some things to consider
#! the retval depends upon the answer to the following question:
        # Are we making a new TCP connection on client side for each of these requests or are we reusing the same old socket used to send the first query from the client to the server?
#!! why no error handling?!
#!!! Is client_socket.close() in specific query handlers undermining the loop of the generic handle_client or not?
# ---------------------------------------------------------docs ? ! 
'''
    ||CLIENT TO SERVER||

action=____&parameter1=_______&parameter2=_______&parameter3=___________

    ||SERVER TO CLIENT||

req&action=________&parameter2=________&parameter2=_____________
 
        ||OR||

res&outcome=___________&parameter1=__________&parameter2=_________________

                ^^
                success/error

'''
# ---------------------------------------------------------------------------------------------------------------------------------
import socket
import sqlite3
import threading
import re
import json

def handle_signup_req(req, client_sock, client_addr, db):
    pos = req.find('username=') + 9
    username = req[pos : req.find('&', pos)]

    pos = req.find('password=') + 9
    password = req[pos:]

    cursor = db.cursor()
    try:
        cursor.execute("INSERT INTO Users (username, password) VALUES (?, ?)", (username, password))
        db.commit()
        response = "res&outcome=success"
        print(f"User signed up: {username}")
    except sqlite3.IntegrityError:
        response = "res&outcome=error&msg=User already exists"
    client_sock.send(response.encode('utf-8'))
    
    return False #!

def handle_login_req(req, client_sock, client_addr, db):
    pos = req.find('username=') + 9
    username = req[pos : req.find('&', pos)]

    pos = req.find('password=') + 9
    password = req[pos:]

    cursor = db.cursor()

    cursor.execute("SELECT password FROM Users WHERE username = ?", (username,))
    row = cursor.fetchone()
    
    if row and row[0] == password:
        response = "res&outcome=success"
        
        # Check if user is already logged in by checking LoggedInUsers table
        cursor.execute("SELECT 1 FROM LoggedInUsers WHERE username = ?", (username,))
        if cursor.fetchone():
            response = "res&outcome=error&msg=You are already logged in on another device"
            print(f"User login error: second device ; username = {username}")
        else:
            # Add user to LoggedInUsers table to mark as logged in
            cursor.execute("INSERT INTO LoggedInUsers (username) VALUES (?)", (username,))#!!
            db.commit()
            print(f"User logged in: {username}")
    else:
        response = "res&outcome=error&msg=Error: Invalid username or password, try again"
    
    client_sock.send(response.encode('utf-8'))
    return False  #!


def handle_logout_req(req, client_sock, client_addr, db):
    pos = req.find('username=') + 9
    username = req[pos : req.find('&', pos)]

    cursor = db.cursor()

    # Remove the user from the LoggedInUsers table if they are logged in
    cursor.execute("DELETE FROM LoggedInUsers WHERE username = ?", (username,))#!!
    db.commit()

    if cursor.rowcount > 0:
        print(f"User logged out ; username = {username}")
        response = "res&outcome=success"
    else:
        response = "res&outcome=error&msg=User not logged in"
    
    client_sock.send(response.encode('utf-8'))
    return False  #!

def handle_create_meeting_req(req, client_sock, client_addr, db):
    pos = req.find('usename=') + 9
    username = req[pos:]

    cursor = db.cursor()

    cursor.execute("INSERT INTO Meetings (username) VALUES (?)", (username,))#!!
    db.commit()
    
    response = "res&outcome=success"
    client_sock.send(response.encode('utf-8'))
    return False #!

def handle_join_meeting_req(req, client_sock, client_addr, db):
    pos = req.find('username=') + 9
    username = req[pos : req.find('&', pos)]

    pos = req.find('mid=') + 4
    mid = int(req[pos:])

    cursor = db.cursor()

    cursor.execute("SELECT * FROM Meetings WHERE mid = (?)", (mid,)) #!!
    if cursor.fetchone():
        cursor.execute("SELECT * FROM Participants WHERE mid = (?)", (mid,)) #!!
        participants = cursor.fetchall()
        
        #tell all participants about the new one
        msg = "req&action=participant_data&data=" + json.dumps([list(row) for row in participants])
        # mid username   ip   port
        # 0   1          2    3
        for p in participants:
            try:
                p_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                p_sock.connect((p[1], int(p[2])))
            except socket.error as err:
                print(f"Error while creating or connecting p_socket: {err}")
                res = "res&outcome=error&msg=failed to join meeting"
                client_sock.send(res.encode('utf-8'))
                client_sock.close()
                return False #!
            
            p_sock.send(msg.encode('utf-8'))
            p_sock.close()

        #tell new one about all others
        client_sock.send(msg)
        success = "res&outcome=success"
        client_sock.send(success.encode('utf-8'))
        client_sock.close()
        #save new one in database
        cursor.execute("INSERT INTO Participants (mid, username, userIp, userPort) VALUES (?)", (mid, username, client_addr[0], client_addr[1])) #!!
        db.commit()
    else:
        failure = "res&outcome=error&msg=meeting doesn't exist"
        client_sock.send(failure.encode('utf-8'))
        client_sock.close() #!!!
    return False #!

def handle_leave_meeting_req(req, client_sock, client_addr, db):
    pos = req.find('username=') + 9
    username = req[pos : req.find('&', pos)]

    pos = req.find('mid=') + 4
    mid = int(req[pos:])

    cursor = db.cursor()
    cursor.execute("SELECT * FROM Meetings WHERE mid = (?)", (mid,))
    if cursor.fetchone():
        cursor.execute("DELETE FROM Participants WHERE mid = (?) AND username = (?)", (mid, username)) #!!
        cursor.commit()
        res = "res&outcome=success"
        client_sock.send(res.encode('utf-8'))
        client_sock.close() #!!!
    else:
        res = "res&outcome=error&msg=invalid mid"
        client_sock.send(res.encode('utf-8'))
        client_socket.close()

    return False #!

req_patterns = {
    re.compile(r'^action=signup&username=\w+&password=.+$') : handle_signup_req,
    re.compile(r'^action=login&username=\w+&password=.+$') : handle_login_req,
    re.compile(r'^action=logout&username=\w+$') : handle_logout_req,
    re.compile(r'^action=create_meeting&username=\w+$') : handle_create_meeting_req,
    re.compile(r'^action=join_meeting&username=\w+&mid=\d+$') : handle_join_meeting_req,
    re.compile(r'^action=leave_meeting&username=\w+&mid=\d+$') : handle_leave_meeting_req
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
                loop_flag = func(request, client_sock, client_addr, db)
                nomatch = False
                break
        
        if nomatch == True:
            error_response = "res&outcome=error&msg=Invalid request format"
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