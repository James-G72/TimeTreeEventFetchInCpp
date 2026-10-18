"""Exercise the actual C++ HTTP client without credentials or TimeTree access.
Usage: python tests/local_api.py ./timetree_integration.exe
"""
import http.server
import json
import os
import re
import subprocess
import sys
import threading

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def reply(self, status, body, cookie=None):
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        if cookie:
            self.send_header("Set-Cookie", cookie)
        self.end_headers()
        self.wfile.write(body.encode())

    def do_PUT(self):
        payload = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        valid = (self.path == "/auth/email/signin"
                 and self.headers.get("User-Agent") == "TimeTreeHandlerCpp/0.1"
                 and self.headers.get("X-Timetreea") == "web/2.1.0/en"
                 and payload.get("uid") == "test-user"
                 and payload.get("password") == "test-password"
                 and re.fullmatch(r"[0-9a-f]{12}4[0-9a-f]{3}[89ab][0-9a-f]{15}", payload.get("uuid", "")))
        self.reply(200 if valid else 400, "{}", "_session_id=test-session; Path=/; HttpOnly" if valid else None)

    def do_GET(self):
        if self.path == "/ping":
            self.reply(200, '{"ok": true}')
        elif self.path == "/calendars?since=0" and self.headers.get("Cookie") == "_session_id=test-session":
            self.reply(200, json.dumps({"calendars": [{"id": 7, "name": "Test", "alias_code": "test",
                "created_at": 1735689600123, "updated_at": 1735776000456,
                "calendar_labels": [{"name": "Work", "color": 42}]}]}))
        elif self.path == "/calendars?since=0" and self.headers.get("Cookie") == "_session_id=invalid-json":
            self.reply(200, "invalid json")
        elif self.path.startswith("/calendar/7/events/sync"):
            cookie = self.headers.get("Cookie")
            if cookie not in ("_session_id=test-session", "_session_id=bad-page", "_session_id=loop", "_session_id=changed"):
                self.reply(401, '{}')
                return
            def event(event_id, title, all_day=False, recurrences=None):
                return {"id": str(event_id), "calendar_id": 7, "title": title,
                        "author_id": 1, "label_id": None,
                        "updated_at": 1735776000000, "start_at": 1735689600000,
                        "end_at": 1735689600000 if all_day else 1735693200000,
                        "all_day": all_day, "recurrences": recurrences or [],
                        "note": "Preserve extra API fields"}
            if self.path == "/calendar/7/events/sync":
                if cookie == "_session_id=changed":
                    self.reply(200, json.dumps({"events": [event(5000000001, "Refreshed title"),
                                                          event(2, "No longer recurring")], "chunk": False}))
                    return
                body = {"events": [event(5000000001, "Old title"),
                                   event(2, "Recurring", True, ["RRULE:FREQ=WEEKLY"])],
                        "chunk": True, "since": 1000}
            elif self.path == "/calendar/7/events/sync?since=1000":
                if cookie == "_session_id=bad-page":
                    self.reply(200, '{"events":{},"chunk":false}')
                    return
                body = {"events": [event(5000000001, "Updated title"), event(3, "New event")],
                        "chunk": cookie == "_session_id=loop", "since": 1000}
            else:
                self.reply(400, '{}')
                return
            self.reply(200, json.dumps(body))
        else:
            self.reply(401, '{"error":"unauthorized"}')

server = http.server.HTTPServer(("127.0.0.1", 0), Handler)
worker = threading.Thread(target=server.serve_forever)
worker.daemon = True
worker.start()
base = "http://127.0.0.1:{}".format(server.server_port)
env = os.environ.copy()
env["TIMETREE_BASE_URL"] = base
try:
    result = subprocess.call([os.path.abspath(sys.argv[1]), base], env=env)
finally:
    server.shutdown()
    server.server_close()
sys.exit(result)
