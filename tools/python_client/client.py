import socket
import struct
import threading
import sys
import time

MSG_SHOT = 0x01
MSG_SHOT_RESULT = 0x02
MSG_PLACE_SHIPS = 0x03
MSG_GAME_START = 0x04
MSG_GAME_OVER = 0x05

MSG_WAITING = 0x0A
MSG_MATCH_FOUND = 0x0B
MSG_PLAYER_NUMBER = 0x0C

MSG_YOUR_TURN = 0x14
MSG_ENEMY_TURN = 0x15
MSG_ENEMY_SHOT = 0x16
MSG_PLACEMENT_READY = 0x17
MSG_BATTLE_START = 0x18
MSG_OPPONENT_DISCONNECTED = 0x19

MSG_ERROR = 0xFF

RESULT_NAMES = {
    0: "MISS",
    1: "HIT",
    2: "SINK",
    3: "ALREADY_SHOT",
}


class BattleShipClient:
    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.sock = None
        self.player_number = 0
        self.my_turn = False
        self.game_over = False

        self.my_board = [[0] * 10 for _ in range(10)]
        self.enemy_board = [[0] * 10 for _ in range(10)]
        self.lock = threading.Lock()

        self.match_found_event = threading.Event()

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))
        print(f"Connected to {self.host}:{self.port}")

    def send_message(self, msg_type, payload=b''):
        header = struct.pack('>BH', msg_type, len(payload))
        self.sock.sendall(header + payload)

    def read_message(self):
        header = self._recv_exact(3)
        if header is None:
            return None
        msg_type = header[0]
        length = struct.unpack('>H', header[1:3])[0]
        payload = self._recv_exact(length)
        if payload is None:
            return None
        return msg_type, payload

    def _recv_exact(self, n):
        data = b''
        while len(data) < n:
            try:
                chunk = self.sock.recv(n - len(data))
                if not chunk:
                    return None
                data += chunk
            except ConnectionError:
                return None
        return data

    def listener_thread(self):
        while not self.game_over:
            msg = self.read_message()
            if msg is None:
                print("\n[Server disconnected]")
                self.game_over = True
                self.match_found_event.set()
                break
            msg_type, payload = msg
            self.handle_message(msg_type, payload)

    def handle_message(self, msg_type, payload):
        with self.lock:
            if msg_type == MSG_GAME_START:
                print("[Server] Welcome to Battleship!")

            elif msg_type == MSG_WAITING:
                print("[Server] Waiting for opponent...")

            elif msg_type == MSG_MATCH_FOUND:
                print("[Server] Match found!")
                self.match_found_event.set()

            elif msg_type == MSG_PLAYER_NUMBER:
                self.player_number = payload[0] if payload else 0
                print(f"[Server] You are Player #{self.player_number}")

            elif msg_type == MSG_PLACEMENT_READY:
                print("[Server] Your ships are placed. Waiting for opponent...")

            elif msg_type == MSG_BATTLE_START:
                print("\n" + "=" * 40)
                print("BATTLE STARTED")
                print("=" * 40)
                self.print_boards()

            elif msg_type == MSG_YOUR_TURN:
                self.my_turn = True
                print("\n>>> YOUR TURN! Enter shot (e.g., A5): ", end='', flush=True)

            elif msg_type == MSG_ENEMY_TURN:
                self.my_turn = False
                print("\n[Enemy's turn...]")

            elif msg_type == MSG_SHOT_RESULT:
                row, col, result = payload[0], payload[1], payload[2]
                result_name = RESULT_NAMES.get(result, "UNKNOWN")
                print(f"\n[Your shot at {self.coord_to_str(row, col)}]: {result_name}")
                if result == 0:
                    self.enemy_board[row][col] = 3
                elif result in (1, 2):
                    self.enemy_board[row][col] = 2
                self.print_boards()

            elif msg_type == MSG_ENEMY_SHOT:
                row, col, result = payload[0], payload[1], payload[2]
                result_name = RESULT_NAMES.get(result, "UNKNOWN")
                print(f"\n[Enemy shot at {self.coord_to_str(row, col)}]: {result_name}")
                if result == 0:
                    self.my_board[row][col] = 3
                elif result in (1, 2):
                    self.my_board[row][col] = 2
                self.print_boards()

            elif msg_type == MSG_GAME_OVER:
                winner = payload[0] if payload else 0
                reason = payload[1] if len(payload) > 1 else 0
                print("\n" + "=" * 40)
                if winner == self.player_number:
                    print("YOU WIN!")
                else:
                    print("YOU LOSE!")
                if reason == 0:
                    print("All enemy ships destroyed!" if winner == self.player_number
                          else "All your ships destroyed!")
                elif reason == 1:
                    print("Opponent disconnected")
                print("=" * 40)
                self.game_over = True

            elif msg_type == MSG_OPPONENT_DISCONNECTED:
                print("\n[Server] Opponent disconnected. You win!")
                self.game_over = True

            elif msg_type == MSG_ERROR:
                error_code = payload[0] if payload else 0
                print(f"\n[Error] Code: {error_code}")

    def send_placement(self):
        ships = [
            (0, 0, 4, 0),
            (2, 0, 3, 0),
            (4, 0, 3, 0),
            (6, 0, 2, 0),
            (8, 0, 2, 0),
            (6, 3, 2, 0),
            (0, 5, 1, 0),
            (2, 4, 1, 0),
            (4, 4, 1, 0),
            (8, 3, 1, 0),
        ]

        payload = bytes([len(ships)])
        for row, col, size, orient in ships:
            payload += bytes([row, col, size, orient])

        self.send_message(MSG_PLACE_SHIPS, payload)
        print("[Client] Ships placement sent")

        for row, col, size, orient in ships:
            for i in range(size):
                if orient == 0:  # HORIZONTAL
                    self.my_board[row][col + i] = 1
                else:  # VERTICAL
                    self.my_board[row + i][col] = 1

    def send_shot(self, row, col):
        payload = bytes([row, col])
        self.send_message(MSG_SHOT, payload)

    def coord_to_str(self, row, col):
        return f"{chr(ord('A') + col)}{row + 1}"

    def str_to_coord(self, s: str):
        s = s.strip().upper()
        if len(s) < 2:
            return None
        col = ord(s[0]) - ord('A')
        try:
            row = int(s[1:]) - 1
        except ValueError:
            return None
        if 0 <= row < 10 and 0 <= col < 10:
            return row, col
        return None

    def print_boards(self):
        print("\n" + "=" * 50)
        print("YOUR FIELD:")
        self._print_board(self.my_board, show_ships=True)
        print("\nENEMY FIELD:")
        self._print_board(self.enemy_board, show_ships=False)
        print("=" * 50)

    def _print_board(self, board, show_ships: bool):
        symbols = {0: '.', 1: 'S', 2: 'X', 3: 'o'}
        print("   " + " ".join(chr(ord('A') + c) for c in range(10)))
        for r in range(10):
            row_str = f"{r+1:2d} "
            for c in range(10):
                cell = board[r][c]
                if cell == 1 and not show_ships:
                    row_str += '. '
                else:
                    row_str += symbols.get(cell, '?') + ' '
            print(row_str)

    def run(self):
        self.connect()

        listener = threading.Thread(target=self.listener_thread, daemon=True)
        listener.start()

        print("Waiting for match...")
        self.match_found_event.wait(timeout=60)

        if self.game_over:
            print("Disconnected before match started.")
            self.sock.close()
            return

        self.send_placement()

        print("\nWaiting for battle to start...")
        print("When it's your turn, enter coordinates like: A5, B3, J10\n")

        while not self.game_over:
            if self.my_turn:
                try:
                    user_input = input()
                    coords = self.str_to_coord(user_input)
                    if coords is None:
                        print("Invalid format. Use: A5, B3, J10")
                        print(">>> YOUR TURN! Enter shot: ", end='', flush=True)
                        continue
                    row, col = coords
                    self.send_shot(row, col)
                    self.my_turn = False
                except EOFError:
                    print("\nExiting...")
                    break
                except KeyboardInterrupt:
                    print("\nExiting...")
                    break
            else:
                time.sleep(0.1)

        self.sock.close()


def main():
    host = "localhost"
    port = 9090

    if len(sys.argv) > 1:
        host = sys.argv[1]
    if len(sys.argv) > 2:
        port = int(sys.argv[2])

    client = BattleShipClient(host, port)

    try:
        client.run()
    except ConnectionRefusedError:
        print(f"Cannot connect to {host}:{port}. Is the server running?")
    except Exception as e:
        print(f"Error: {e}")


if __name__ == "__main__":
    main()