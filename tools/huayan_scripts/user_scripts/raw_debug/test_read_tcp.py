import socket

HOST = "0.0.0.0"
PORT = 5000

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((HOST, PORT))
    s.listen(1)

    conn, addr = s.accept()

    with conn, open("received.py", "w", encoding="utf-8") as f:
        buffer = ""

        while True:
            data = conn.recv(4096)

            if not data:
                break

            buffer += data.decode("utf-8")

            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)

                # print(f"RECV: {line}")

                f.write(line + "\n")

        if buffer:
            f.write(buffer)

print("Done")