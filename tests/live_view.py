"""Check timed refresh, recovery, edited titles, and deletion display locally.
Usage: python tests/live_view.py ./timetree_example.exe
Runs for about 23 seconds; never contacts TimeTree.
"""
import datetime
import http.server
import json
import os
import re
import subprocess
import sys
import threading

now = int(datetime.datetime.now(datetime.timezone.utc).timestamp() * 1000)

class Handler(http.server.BaseHTTPRequestHandler):
    syncs = 0
    logins = 0

    def log_message(self, *args):
        pass

    def reply(self, status, body, cookie=None):
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        if cookie:
            self.send_header('Set-Cookie', cookie)
        self.end_headers()
        self.wfile.write(json.dumps(body).encode())

    def do_PUT(self):
        payload = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        valid = (self.path == '/auth/email/signin'
                 and self.headers.get('User-Agent') == 'TimeTreeHandlerCpp/0.1'
                 and self.headers.get('X-Timetreea') == 'web/2.1.0/en'
                 and self.headers.get('Content-Type') == 'application/json'
                 and payload.get('uid') == 'test-user'
                 and payload.get('password') == 'test-password'
                 and re.fullmatch(r'[0-9a-f]{12}4[0-9a-f]{3}[89ab][0-9a-f]{15}', payload.get('uuid', '')))
        if not valid:
            self.reply(400, {})
            return
        Handler.logins += 1
        self.reply(200, {}, '_session_id=live-test; Path=/; HttpOnly')

    def do_GET(self):
        if self.headers.get('Cookie') != '_session_id=live-test':
            self.reply(401, {})
        elif self.path == '/calendars?since=0':
            self.reply(200, {'calendars': [{'id': 7, 'name': 'Ruth', 'created_at': now - 86400000,
                'calendar_users': [{'user_id': 1, 'name': 'Test author'}],
                'calendar_labels': [{'id': 4, 'name': 'Test label', 'color': 42}]}]})
        elif self.path == '/calendar/7/events/sync':
            Handler.syncs += 1
            if Handler.syncs == 1:
                self.reply(401, {})
                return
            if Handler.syncs == 3:
                self.reply(500, {})
                return
            def event(event_id, title):
                return {'id': event_id, 'calendar_id': 7, 'title': title, 'author_id': 1,
                        'label_id': 4, 'updated_at': now, 'start_at': now, 'end_at': now + 3600000,
                        'all_day': False, 'recurrences': []}
            events = [event('main', 'Original title' if Handler.syncs == 2 else 'Edited live title')]
            if Handler.syncs == 2:
                events.append(event('removed', 'Event removed live'))
            self.reply(200, {'events': events, 'chunk': False})
        else:
            self.reply(404, {})

server = http.server.HTTPServer(('127.0.0.1', 0), Handler)
worker = threading.Thread(target=server.serve_forever)
worker.daemon = True
worker.start()
env = os.environ.copy()
env['TIMETREE_BASE_URL'] = 'http://127.0.0.1:{}'.format(server.server_port)
env['TIMETREE_USERNAME'] = 'test-user'
env['TIMETREE_PASSWORD'] = 'test-password'
process = subprocess.Popen([os.path.abspath(sys.argv[1])], env=env,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
try:
    try:
        output, _ = process.communicate(timeout=23)
        raise AssertionError('Live viewer exited unexpectedly: ' + output)
    except subprocess.TimeoutExpired:
        process.terminate()
        output, _ = process.communicate(timeout=5)
    for expected in ('Update 0', 'Original title', 'Refresh failed:', 'Update 1',
                     'Edited live title', 'Deleted since this program started: 1',
                     'Event removed live', 'Test author', 'Test label', 'Press Ctrl+C to stop.'):
        if expected not in output:
            raise AssertionError('Missing {!r} in output:\n{}'.format(expected, output))
    if Handler.logins != 2:
        raise AssertionError('Expected initial login and one session renewal')
    print('Desktop HTTP checks passed: login payload/headers, session renewal, live edits and recovery')
finally:
    if process.poll() is None:
        process.kill()
        process.wait()
    server.shutdown()
    server.server_close()
