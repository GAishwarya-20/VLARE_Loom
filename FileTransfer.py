import socket
import os
import time

# --- Configuration ---
esp_ip = "192.168.4.10"   # ESP32 AP IP
esp_port = 21234          # Port
filename = r"C:\\Users\\Gandeevan\\Downloads\\border d 1.bmp" # IMPORTANT: Change this path

def receive_line(sock):
    """Helper function to receive data from a socket until a newline is found."""
    data = b""
    while not data.endswith(b"\n"):
        try:
            chunk = sock.recv(1)
            if not chunk:
                return None
            data += chunk
        except socket.timeout:
            return None
    return data

def transfer_file():
    """Main function to handle the entire file transfer logic."""
    
    if not os.path.exists(filename):
        print(f"Error: File not found at '{filename}'")
        return False
        
    file_size = os.path.getsize(filename)
    print(f"File: '{os.path.basename(filename)}', Size: {file_size} bytes")

    try:
        print(f"Connecting to {esp_ip}:{esp_port}...")
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5.0)
        s.connect((esp_ip, esp_port))
        print("Connected.")

        # --- Step 1: Send Start Command ---
        print("Sending start command...")
        s.sendall(b"CMD_START_FILE_TRANSFER\n")

        # --- Step 2: Wait for Start SUCCESS ---
        print("Waiting for START_FILE_TRANSFER SUCCESS...")
        response = receive_line(s)
        if not response:
            print("Error: No response from server (timeout).")
            s.close()
            return False

        response_str = response.decode(errors="ignore").strip()

        if response_str != "START_FILE_TRANSFER SUCCESS":
            print(f"Error: Expected 'START_FILE_TRANSFER SUCCESS' but got '{response_str}'")
            s.close()
            return False
        print("Received START_FILE_TRANSFER SUCCESS. Ready to send file.")

        # --- Step 3 & 4: Send File Data then End Command ---
        print("Sending file data...")
        with open(filename, "rb") as f:
            data = f.read()
            s.sendall(data)

        s.sendall(b"CMD_END_FILE_TRANSFER\n")
        print("File data and end command sent.")
        
        # --- Step 5: Wait for End SUCCESS with size ---
        print("Waiting for END_FILE_TRANSFER SUCCESS with file size...")
        response_line = receive_line(s)
        if not response_line:
            print("Error: Did not receive final acknowledgement from server.")
            s.close()
            return False

        response_str = response_line.decode(errors="ignore").strip()

        # --- Step 6: Verify File Size ---
        if response_str.startswith("END_FILE_TRANSFER SUCCESS:"):
            try:
                received_size_str = response_str.split(':')[1]
                received_size = int(received_size_str)
                
                if received_size == file_size:
                    print(f"\nSUCCESS ✅: Server confirmed receiving {received_size} bytes. Matches sent size.")
                    s.close()
                    return True
                else:
                    print(f"\nFAILURE ❌: Size mismatch! Sent {file_size} bytes, but server received {received_size} bytes.")
                    s.close()
                    return False
            except (IndexError, ValueError) as e:
                print(f"Error: Could not parse server response. Error: {e}")
                s.close()
                return False
        elif response_str.startswith("END_FILE_TRANSFER FAILURE"):
            print("FAILURE ❌: Server reported file transfer failed.")
            s.close()
            return False
        else:
            print(f"Error: Expected 'END_FILE_TRANSFER SUCCESS:<size>' but got '{response_str}'")
            s.close()
            return False

    except socket.timeout:
        print("Error: Connection timed out.")
        return False
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        return False

if __name__ == "__main__":
    print("\n--- File Transfer ---")
    transfer_file()
