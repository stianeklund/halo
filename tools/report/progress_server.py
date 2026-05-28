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
import threading
from http.server import HTTPServer, SimpleHTTPRequestHandler
from socketserver import ThreadingMixIn

# Scoring lock: one objdiff run at a time per unit
_score_locks = {}
_score_locks_mu = threading.Lock()

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)


def _function_symbol_aliases(func):
    """Return likely objdiff symbol names for a report function entry."""
    aliases = []
    name = func.get('name')
    if name:
        aliases.append(name)

    address = func.get('address')
    if isinstance(address, str):
        try:
            addr = int(address, 0)
        except ValueError:
            addr = None
        if addr is not None:
            suffix = f'{addr:08x}'
            aliases.append(f'FUN_{suffix}')
            aliases.append(f'thunk_FUN_{suffix}')

    return aliases


def _lookup_score_for_function(func, func_scores):
    for alias in _function_symbol_aliases(func):
        if alias in func_scores:
            return func_scores[alias]
    return None


def _function_reference_unit_name(func):
    address = func.get('address')
    if not isinstance(address, str):
        return None
    try:
        return f'FUN_{int(address, 0):08x}'
    except ValueError:
        return None


def _score_function_from_reference_unit(tracker, func):
    """Fallback to the per-function reference unit for thunked/mis-grouped symbols."""
    unit_name = _function_reference_unit_name(func)
    if not unit_name:
        return None

    config = tracker._get_unit_config(unit_name)
    if not config:
        return None

    aliases = tracker._get_unit_symbols(config['base_path'])
    if not aliases:
        return None

    result = tracker.check_unit(
        unit_name,
        force=True,
        symbol_aliases={func['name']: aliases},
    )
    if not result:
        return None

    scores = {entry.get('name'): entry.get('match') for entry in result.get('functions', [])}
    return scores.get(func['name'])


class SSEHandler(SimpleHTTPRequestHandler):
    """HTTP handler that also serves an SSE endpoint at /events."""

    def do_GET(self):
        if self.path == '/events':
            self.handle_sse()
        else:
            try:
                super().do_GET()
            except (BrokenPipeError, ConnectionResetError, OSError):
                # Clients can disconnect mid-response (e.g. browser refresh/abort).
                # Treat this as normal and avoid noisy socketserver tracebacks.
                pass

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()

    def do_POST(self):
        if self.path == '/api/score':
            self.handle_score()
        else:
            self.send_error(404, 'Not found')

    def _json_response(self, status, body):
        data = json.dumps(body).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(data)

    def handle_score(self):
        try:
            length = int(self.headers.get('Content-Length', 0))
            body = json.loads(self.rfile.read(length) if length else b'{}')
        except (ValueError, json.JSONDecodeError) as e:
            self._json_response(400, {'error': f'Bad request: {e}'})
            return

        unit_name = body.get('unit')
        if not unit_name:
            self._json_response(400, {'error': 'Missing "unit" field'})
            return

        # Per-unit lock so we don't run two objdiff instances for the same unit
        with _score_locks_mu:
            if unit_name not in _score_locks:
                _score_locks[unit_name] = threading.Lock()
            lock = _score_locks[unit_name]

        with lock:
            result = self._run_score(unit_name)

        if result is None:
            self._json_response(404, {'error': 'no_reference', 'unit': unit_name})
            return

        self._json_response(200, result)

    def _run_score(self, unit_name):
        """Run objdiff scoring for unit_name, update report.json in-place, return scores dict."""
        import sys as _sys
        script_dir = os.path.dirname(os.path.abspath(__file__))
        _sys.path.insert(0, script_dir)
        from matching import MatchingTracker

        # Collect ported function names from report so we only diff those
        report_path = os.path.join(self.directory, 'report.json')
        ported_funcs = None
        try:
            with open(report_path) as f:
                report = json.load(f)
            for unit in report.get('units', []):
                if unit['name'] == unit_name:
                    ported_funcs = [
                        fn for fn in unit.get('functions', [])
                        if fn.get('ported')
                    ]
                    break
        except Exception:
            report = None

        ported_symbol_aliases = None
        if ported_funcs:
            ported_symbol_aliases = {
                func['name']: _function_symbol_aliases(func)
                for func in ported_funcs
                if func.get('name')
            }

        tracker = MatchingTracker()
        unit_data = tracker.check_unit(
            unit_name,
            force=True,
            symbol_aliases=ported_symbol_aliases,
        )
        if unit_data is None:
            logging.warning('Scoring failed for unit %s (no reference or objdiff error)', unit_name)
            return None

        tracker.save_cache()

        if report is None:
            logging.error('Cannot read report.json')
            return None

        # Build func-name → match_percent from objdiff results
        func_scores = {}
        for fn in unit_data.get('functions', []):
            fname = fn.get('name')
            if fname:
                func_scores[fname] = fn.get('match', 0)

        # Update matching unit in report
        updated_funcs = {}
        for unit in report.get('units', []):
            if unit['name'] == unit_name:
                for func in unit.get('functions', []):
                    fname = func.get('name')
                    score = _lookup_score_for_function(func, func_scores)
                    if score is None and func.get('ported') and fname:
                        score = _score_function_from_reference_unit(tracker, func)
                    if score is not None:
                        func['match_percent'] = round(score, 2)
                        updated_funcs[fname] = func['match_percent']
                break

        try:
            with open(report_path, 'w') as f:
                json.dump(report, f)
            os.utime(report_path, None)  # ensure mtime bumped for SSE polling
        except Exception as e:
            logging.error('Cannot write report.json: %s', e)
            return None

        logging.info('Scored unit %s: %d functions updated', unit_name, len(updated_funcs))
        return {'ok': True, 'unit': unit_name, 'scores': updated_funcs}

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
