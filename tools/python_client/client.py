import socket
import struct
import threading
import sys
import time
import getpass

PROTOCOL_VERSION = 0x07

# ------- protocol messages -------
MSG_SHOT                    = 0x01
MSG_SHOT_RESULT             = 0x02
MSG_PLACE_SHIPS             = 0x03
MSG_GAME_START              = 0x04
MSG_GAME_OVER               = 0x05

MSG_WAITING                 = 0x0A
MSG_MATCH_FOUND             = 0x0B
MSG_PLAYER_NUMBER           = 0x0C
MSG_REMATCH_REQUEST         = 0x2E
MSG_REMATCH_OFFER           = 0x2F
MSG_REMATCH_DECLINE         = 0x30
MSG_REMATCH_DECLINED        = 0x31
MSG_REMATCH_START           = 0x32

MSG_ROOM_CREATE             = 0x28
MSG_ROOM_CREATED            = 0x29
MSG_ROOM_LIST_REQUEST       = 0x2A
MSG_ROOM_LIST               = 0x2B
MSG_ROOM_JOIN               = 0x2C
MSG_ROOM_JOIN_FAIL          = 0x2D

MSG_YOUR_TURN               = 0x14
MSG_ENEMY_TURN              = 0x15
MSG_ENEMY_SHOT              = 0x16
MSG_PLACEMENT_READY         = 0x17
MSG_BATTLE_START            = 0x18
MSG_OPPONENT_DISCONNECTED   = 0x19
MSG_PLACE_RANDOM            = 0x22
MSG_OWN_SHIPS               = 0x23
MSG_BOARD_UPDATE            = 0x24
MSG_TURN_TIMEOUT            = 0x25

MSG_AUTH_REGISTER           = 0x1E
MSG_AUTH_LOGIN              = 0x1F
MSG_AUTH_OK                 = 0x20
MSG_AUTH_FAIL               = 0x21

MSG_ERROR                   = 0xFF
# ---------------------------------

RESULT_NAMES = {
    0: "MISS",
    1: "HIT",
    2: "SINK",
    3: "ALREADY_SHOT",
}

AUTH_ERRORS = {
    1: "Wrong username or password",
    2: "Username already taken",
    3: "Malformed request",
    4: "Authentication required first",
    5: "Invalid username (3-16 chars: latin, digits, _)",
    6: "This account is already online",
}


class BattleShipClient:
    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.sock = None
        self.player_number = 0
        self.my_turn = False
        self.game_over = False
        self.username = ""
        self.in_room = False
        self.quit = False 
        self.rematch_started = False
        self.rematch_decision_event = threading.Event()
        self.auth_event = threading.Event()
        self.auth_failed = False
        self.my_board = [[0] * 10 for _ in range(10)]
        self.enemy_board = [[0] * 10 for _ in range(10)]
        self.opponent_name = ""
        self.lock = threading.Lock()
        self.match_found_event = threading.Event()
        self.welcome_event = threading.Event()
        self.placement_ready_event = threading.Event() 
        self.battle_start_event = threading.Event()

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
        while not self.quit:
            msg = self.read_message()
            if msg is None:
                print("\n[Server disconnected]")
                self.game_over = True
                self.quit = True
                self.welcome_event.set()
                self.match_found_event.set()
                self.placement_ready_event.set()
                self.battle_start_event.set()
                self.rematch_decision_event.set()
                break
            msg_type, payload = msg
            self.handle_message(msg_type, payload)

    def send_credentials(self, msg_type, username, password):
        name_b = username.encode()
        pass_b = password.encode()
        payload = bytes([len(name_b)]) + name_b + bytes([len(pass_b)]) + pass_b
        self.send_message(msg_type, payload)

    def handle_message(self, msg_type, payload):
        with self.lock:
            if msg_type == MSG_GAME_START:
                version = payload[0] if payload else 0
                if version != PROTOCOL_VERSION:
                    print(f"[Server] Protocol version mismatch: server v{version}, "
                        f"client v{PROTOCOL_VERSION}. Refusing to connect.")
                    self.game_over = True
                    self.welcome_event.set()
                    return
                print("[Server] Welcome to Battleship!")
                self.welcome_event.set()

            elif msg_type == MSG_WAITING:
                print("[Server] Waiting for opponent...")

            elif msg_type == MSG_MATCH_FOUND:
                if payload:
                    name_len = payload[0]
                    self.opponent_name = payload[1:1 + name_len].decode(
                        'utf-8', errors='replace')
                    print(f"[Server] Match found! Your opponent: {self.opponent_name}")
                else:
                    print("[Server] Match found!")
                self.match_found_event.set()

            elif msg_type == MSG_PLAYER_NUMBER:
                self.player_number = payload[0] if payload else 0
                print(f"[Server] You are Player #{self.player_number}")

            elif msg_type == MSG_PLACEMENT_READY:
                print("[Server] Your fleet is accepted.")
                self.placement_ready_event.set()

            elif msg_type == MSG_BATTLE_START:
                self.battle_start_event.set()
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
                elif reason == 2:
                    print("Opponent exceeded turn time limit" 
                          if winner == self.player_number 
                          else "You exceeded the turn time limit")
                print("=" * 40)
                self.placement_ready_event.set()
                self.battle_start_event.set()
                self.game_over = True

            elif msg_type == MSG_OPPONENT_DISCONNECTED:
                print("\n[Server] Opponent disconnected. You win!")
                self.placement_ready_event.set()
                self.battle_start_event.set()
                self.game_over = True

            elif msg_type == MSG_AUTH_OK:
                name_len = payload[0]
                name = payload[1:1 + name_len].decode()
                self.username = name                      # ← добавили
                wins, losses = struct.unpack('<II', payload[1 + name_len:1 + name_len + 8])
                print(f"[Server] Authenticated as {name} | wins: {wins}, losses: {losses}")
                self.auth_event.set()

            elif msg_type == MSG_AUTH_FAIL:
                code = payload[0] if payload else 0
                print(f"[Server] Auth failed: {AUTH_ERRORS.get(code, 'unknown code')}")
                self.auth_failed = True
                self.auth_event.set()
                self.game_over = True

            elif msg_type == MSG_OWN_SHIPS:
                count = payload[0]
                print(f"[Server] Your fleet ({count} ships):")
                for i in range(count):
                    row, col, size, orient = payload[1 + i * 4: 5 + i * 4]
                    for k in range(size):
                        if orient == 0:   # HORIZONTAL
                            self.my_board[row][col + k] = 1
                        else:             # VERTICAL
                            self.my_board[row + k][col] = 1
                    print(f"  {self.coord_to_str(row, col)} size={size} "
                        f"{'H' if orient == 0 else 'V'}")
                self.print_boards()

            elif msg_type == MSG_BOARD_UPDATE:
                target = payload[0]
                count = payload[1]
                board = (self.my_board if target == self.player_number else self.enemy_board)
                for i in range(count):
                    row, col, state = payload[2 + i * 3: 5 + i * 3]
                    board[row][col] = state
                print(f"[Server] Sunk ship outlined: {count} cells marked")
                self.print_boards()

            elif msg_type == MSG_TURN_TIMEOUT:
                offender = payload[0] if payload else 0
                who = ("You" if offender == self.player_number else f"Opponent ({self.opponent_name or '?'})")
                print(f"\n[Server] {who} ran out of turn time - turn passed")

            elif msg_type == MSG_ROOM_CREATED:
                room_id = struct.unpack('<H', payload[0:2])[0]
                self.in_room = True
                print(f"[Server] Room #{room_id} created. Waiting for an opponent...")

            elif msg_type == MSG_ROOM_LIST:
                count = payload[0]
                print(f"\n=== Open rooms ({count}) ===")
                off = 1
                for _ in range(count):
                    rid = struct.unpack('<H', payload[off:off + 2])[0]; off += 2
                    nlen = payload[off]; off += 1
                    rname = payload[off:off + nlen].decode(); off += nlen
                    olen = payload[off]; off += 1
                    oname = payload[off:off + olen].decode(); off += olen
                    print(f"  #{rid:<4} {rname:<24} by {oname}")
                if count == 0:
                    print("  (empty — create one with 'c')")

            elif msg_type == MSG_ROOM_JOIN_FAIL:
                codes = {1: "Room not found", 2: "Room already in game",
                        3: "You are already in a room or game",
                        4: "That is your own room"}
                print(f"[Server] Join failed: {codes.get(payload[0], '?')}")

            elif msg_type == MSG_REMATCH_OFFER:
                print("\n[Server] Opponent offers a rematch")

            elif msg_type == MSG_REMATCH_DECLINED:
                print("\n[Server] Opponent declined the rematch")
                self.rematch_started = False
                self.rematch_decision_event.set()

            elif msg_type == MSG_REMATCH_START:
                print("\n[Server] Rematch! Place your fleet again")
                self.my_board = [[0] * 10 for _ in range(10)]
                self.enemy_board = [[0] * 10 for _ in range(10)]
                self.my_turn = False
                self.game_over = False
                self.rematch_started = True
                self.placement_ready_event.clear()
                self.battle_start_event.clear()
                self.rematch_decision_event.set()

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
        enemy_title = (f"ENEMY FIELD ({self.opponent_name}):"
                       if self.opponent_name else "ENEMY FIELD:")
        print("\n" + enemy_title)
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

    def lobby_loop(self):
        while not self.quit and not self.match_found_event.is_set():
            print("\n=== LOBBY ===  [c]reate  [l]ist  [j <id>] join  [q]uit")
            cmd = input("> ").strip()
            if cmd == 'c':
                name = input("Room name: ").strip() or f"{self.username}'s room"
                priv = input("Private? [y/N]: ").strip().lower() == 'y'
                nb = name.encode()
                self.send_message(MSG_ROOM_CREATE,
                                  bytes([len(nb)]) + nb + bytes([1 if priv else 0]))
            elif cmd == 'l':
                self.send_message(MSG_ROOM_LIST_REQUEST)
            elif cmd.startswith('j'):
                parts = cmd.split()
                if len(parts) == 2 and parts[1].isdigit():
                    self.send_message(MSG_ROOM_JOIN, struct.pack('<H', int(parts[1])))
                else:
                    print("Usage: j <room_id>")
            elif cmd == 'q':
                self.quit = True
            time.sleep(0.3)
            if self.in_room and not self.match_found_event.is_set():
                self.match_found_event.wait(timeout=300)

    def play_round(self):
        mode = input("Fleet placement: [r]andom or [f]ixed? [r]: ").strip().lower() or 'r'
        if mode == 'r':
            self.send_message(MSG_PLACE_RANDOM)
            print("[Client] Requested random fleet")
        else:
            self.send_placement()

        if not self.placement_ready_event.wait(timeout=10) or self.quit:
            print("Placement not confirmed, exiting.")
            self.quit = True
            return
        print("[Client] Waiting for opponent's fleet...")
        if not self.battle_start_event.wait(timeout=300) or self.quit:
            print("Battle did not start, exiting.")
            self.quit = True
            return

        print("When it's your turn, enter coordinates like: A5, B3, J10\n")
        while not self.game_over and not self.quit:
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
                    self.quit = True
                    break
                except KeyboardInterrupt:
                    self.quit = True
                    break
            else:
                time.sleep(0.1)

    def run(self):
        self.connect()
        listener = threading.Thread(target=self.listener_thread, daemon=True)
        listener.start()

        if not self.welcome_event.wait(timeout=10) or self.game_over:
            print("No welcome from server or version mismatch, exiting.")
            self.sock.close()
            return

        mode = input("Login or register? [l/r]: ").strip().lower()
        username = input("Username: ").strip()
        password = getpass.getpass("Password: ")
        self.send_credentials(
            MSG_AUTH_REGISTER if mode == 'r' else MSG_AUTH_LOGIN,
            username, password)
        if not self.auth_event.wait(timeout=10) or self.auth_failed:
            print("Authentication failed, exiting.")
            self.sock.close()
            return

        self.lobby_loop()
        if self.quit:
            self.sock.close()
            return

        while not self.quit:
            self.play_round()
            if self.quit:
                break

            ans = input("Rematch? [y/n]: ").strip().lower()
            if ans != 'y':
                self.send_message(MSG_REMATCH_DECLINE)
                break

            self.send_message(MSG_REMATCH_REQUEST)
            print("[Client] Waiting for opponent's decision...")
            if not self.rematch_decision_event.wait(timeout=60):
                print("[Client] No decision from opponent, exiting")
                break
            self.rematch_decision_event.clear()
            if not self.rematch_started:
                break

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