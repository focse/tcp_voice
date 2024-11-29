import socket
import base64
import pyaudio

chunk = 6000


def play_audio(data):
    p = pyaudio.PyAudio()
    stream = p.open(format=pyaudio.paInt16, channels=1, rate=16000, output=True)
    stream.write(data)
    stream.stop_stream()
    stream.close()
    p.terminate()


def start_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind(("0.0.0.0", 8085))
    server.listen(1)
    print("Server listening on port 8085")

    while True:
        client_socket, addr = server.accept()
        print(f"Accepted connection from {addr}")
        data = client_socket.recv(chunk)
        total_data = data
        while len(data) > 0:
            data = client_socket.recv(chunk)
            if len(data) > 0:
                total_data += data

        print(len(total_data))

        # Decode and play audio
        decoded_data = base64.b64decode(total_data)
        # print(decoded_data)
        play_audio(decoded_data)
        client_socket.close()


if __name__ == "__main__":
    start_server()
