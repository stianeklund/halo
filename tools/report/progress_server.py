#!/usr/bin/env python3
"""
SSE-powered live progress dashboard server.

Serves static files from artifacts/progress/ and pushes live updates
via Server-Sent Events whenever report.json changes.

Usage:
    python3 tools/report/progress_server.py [--port 8080] [--directory artifacts/progress]
"""

import os
import sys
import time
import json
import argparse
import logging
from http.server import HTTPServer, SimpleHTTPRequestHandler
from socketserver import ThreadingMixIn

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)


class SSEHandler(SimpleHTTPRequestHandler):
    """HTTP handler that also serves an SSE endpoint at /events."""

    def do_GET(self):
        if self.path == '/events':
            self.handle_sse()
        else:
            super().do_GET()

    def log_message(self, format, *args):
        logging.info("%s - %s", self.client_address[0], format % args)

    def handle_sse(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text/event-stream')
        self.send_header('Cache-Control', 'no-cache, no-transform')
        self.send_header('Connection', 'keep-alive')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('X-Accel-Buffering', 'no')
        self.end_headers()

        report_path = os.path.join(self.directory, 'report.json')
        # Also watch history.json
        history_path = os.path.join(self.directory, 'history.json')
        watched = {
            report_path: 0,
            history_path: 0,
        }

        logging.info('SSE client connected')

        def send_field(name, data):
            msg = f'event: {name}\ndata: {data}\n\n'
            try:
                self.wfile.write(msg.encode('utf-8'))
                self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, OSError):
                return False
            return True

        try:
            # Send initial state
            for path in (report_path, history_path):
                if os.path.exists(path):
                    with open(path) as f:
                        content = f.read()
                    name = 'report' if 'report' in path else 'history'
                    watched[path] = os.path.getmtime(path)
                    if not send_field(name, content):
                        return

            # Poll for changes
            while True:
                time.sleep(1)
                for path in list(watched):
                    if os.path.exists(path):
                        mtime = os.path.getmtime(path)
                        if mtime > watched[path]:
                            watched[path] = mtime
                            with open(path) as f:
                                content = f.read()
                            name = 'report' if 'report' in path else 'history'
                            if not send_field(name, content):
                                return
                            logging.info('Pushed update for %s', os.path.basename(path))
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass
        finally:
            logging.info('SSE client disconnected')


class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    parser = argparse.ArgumentParser(
        description='SSE-powered live progress dashboard server'
    )
    parser.add_argument(
        '--port', type=int, default=8080,
        help='Port to serve on (default: 8080)'
    )
    parser.add_argument(
        '--directory', default='artifacts/progress',
        help='Directory to serve (default: artifacts/progress)'
    )
    parser.add_argument(
        '--host', default='0.0.0.0',
        help='Host to bind to (default: 0.0.0.0)'
    )
    args = parser.parse_args()

    # Change to project root
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(script_dir, '../..'))
    os.chdir(project_root)

    serve_dir = os.path.abspath(args.directory)

    if not os.path.exists(serve_dir):
        logging.error('Directory does not exist: %s', serve_dir)
        sys.exit(1)

    class Handler(SSEHandler):
        def __init__(self, *a, **kw):
            super().__init__(*a, directory=serve_dir, **kw)

    server = ThreadedHTTPServer((args.host, args.port), Handler)
    logging.info('Serving %s', serve_dir)
    logging.info('  Dashboard:  http://localhost:%d', args.port)
    logging.info('  SSE events: http://localhost:%d/events', args.port)
    logging.info('  Press Ctrl+C to stop')

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        logging.info('Shutting down...')
        server.shutdown()


if __name__ == '__main__':
    main()
