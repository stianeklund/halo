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
import subprocess
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




def _recompute_unit_match(unit: dict) -> None:
    """Recompute unit['summary'] match_avg/match_weighted from its function scores in-place."""
    scores = []
    weighted_sum = 0.0
    weighted_bytes = 0
    for func in unit.get('functions', []):
        mp = func.get('match_percent')
        if mp is None:
            continue
        size = func.get('size') or 0
        scores.append(mp)
        weighted_sum += mp * size
        weighted_bytes += size
    summary = unit.setdefault('summary', {})
    if scores:
        summary['match_avg'] = round(sum(scores) / len(scores), 1)
        summary['match_weighted'] = round(weighted_sum / max(weighted_bytes, 1), 1)
    else:
        summary['match_avg'] = None
        summary['match_weighted'] = None


def _recompute_summary_match(report: dict) -> None:
    """Recompute report['summary']['match'] from all units' function scores in-place."""
    total_sum = 0.0
    weighted_sum = 0.0
    weighted_bytes = 0
    count = 0
    for unit in report.get('units', []):
        for func in unit.get('functions', []):
            mp = func.get('match_percent')
            if mp is None:
                continue
            size = func.get('size') or 0
            total_sum += mp
            weighted_sum += mp * size
            weighted_bytes += size
            count += 1
    if count > 0:
        report.setdefault('summary', {})['match'] = {
            'average': round(total_sum / count, 1),
            'weighted': round(weighted_sum / max(weighted_bytes, 1), 1),
            'scored_count': count,
        }


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
        """Run vc71_verify for unit_name, update report.json in-place, return scores dict.

        Uses MSVC 7.1 compilation (via vc71_regression.run_vc71_verify) so the score
        matches the dashboard label: 'compiled with MSVC 7.1 vs original Xbox binary'.
        """
        report_path = os.path.join(self.directory, 'report.json')
        try:
            with open(report_path) as f:
                report = json.load(f)
        except Exception as e:
            logging.error('Cannot read report.json: %s', e)
            return None

        # Look up unit in objdiff.json to get the source file path and delinked ref.
        try:
            with open('objdiff.json') as f:
                objdiff_config = json.load(f)
        except Exception as e:
            logging.error('Cannot read objdiff.json: %s', e)
            return None

        unit_config = None
        for entry in objdiff_config.get('units', []):
            if entry['name'] == unit_name or entry['name'].endswith(f'/{unit_name}'):
                unit_config = entry
                break

        if not unit_config:
            logging.warning('Unit %s not found in objdiff.json', unit_name)
            return None

        source_path_rel = unit_config.get('metadata', {}).get('source_path')
        base_path = unit_config.get('base_path')

        if not source_path_rel:
            logging.warning('No source_path in objdiff.json metadata for unit %s', unit_name)
            return None
        if not base_path or not os.path.exists(base_path):
            logging.warning('No delinked reference for unit %s (%s)', unit_name, base_path)
            return None

        # Import run_vc71_verify from tools/verify/vc71_regression.py
        import sys as _sys
        from pathlib import Path
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.abspath(os.path.join(script_dir, '../..'))
        verify_dir = os.path.join(project_root, 'tools', 'verify')
        if verify_dir not in _sys.path:
            _sys.path.insert(0, verify_dir)
        try:
            from vc71_regression import (
                run_vc71_verify as _run_vc71_verify,
                load_baseline as _load_baseline,
                save_baseline as _save_baseline,
            )
        except ImportError as e:
            logging.error('Cannot import vc71_regression: %s', e)
            return None

        source_path = Path(project_root) / source_path_rel
        if not source_path.exists():
            logging.warning('Source file not found for unit %s: %s', unit_name, source_path)
            return None

        logging.info('Running vc71_verify for %s ...', source_path_rel)
        try:
            vc71_results = _run_vc71_verify(source_path)
        except Exception as e:
            logging.error('vc71_verify failed for unit %s: %s', unit_name, e)
            return None

        if not vc71_results:
            logging.warning('vc71_verify produced no results for unit %s', unit_name)
            return None

        # 1) Persist to vc71_scores.json — the SOURCE OF TRUTH the report generator
        #    reads. Mirror the regression policy: raise an existing floor or add a new
        #    entry, never silently lower (that would mask a regression). Without this
        #    write the score is ephemeral: the next report regeneration would wipe it.
        baseline = _load_baseline()
        baseline_changed = False
        for fn_name, info in vc71_results.items():
            new_score = info['score']
            old = baseline.get(fn_name)
            if old is None or new_score > old.get('score', -1) + 0.1:
                baseline[fn_name] = {'score': new_score, 'source': source_path_rel}
                baseline_changed = True
        if baseline_changed:
            _save_baseline(baseline)

        # 2) Mirror the now-persisted scores into the in-memory report. Join by name
        #    first, then by the address-keyed FUN_<addr> alias (vc71_verify records
        #    some functions under their delinked reference name) — identical to the
        #    generator's join so a later regeneration produces the same numbers.
        def _score_for(func):
            addr = func.get('address')
            addr_int = None
            if isinstance(addr, str):
                try:
                    addr_int = int(addr, 16)
                except ValueError:
                    addr_int = None
            candidates = [func.get('name')]
            if addr_int is not None:
                candidates += [f'FUN_{addr_int:08x}', f'FUN_{addr_int:08X}',
                               f'thunk_FUN_{addr_int:08x}']
            for key in candidates:
                if key and key in vc71_results:
                    return vc71_results[key]['score']
            return None

        updated_funcs = {}
        for unit in report.get('units', []):
            if unit['name'] == unit_name:
                for func in unit.get('functions', []):
                    score = _score_for(func)
                    if score is not None:
                        func['match_percent'] = round(score, 2)
                        updated_funcs[func.get('name')] = func['match_percent']
                break

        # Recompute unit summary and global summary.
        for unit in report.get('units', []):
            if unit['name'] == unit_name:
                _recompute_unit_match(unit)
                break
        _recompute_summary_match(report)

        try:
            with open(report_path, 'w') as f:
                json.dump(report, f)
            os.utime(report_path, None)  # ensure mtime bumped for SSE polling
        except Exception as e:
            logging.error('Cannot write report.json: %s', e)
            return None

        logging.info('VC71-scored unit %s: %d functions updated', unit_name, len(updated_funcs))
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


def _background_regen(serve_dir: str, project_root: str, interval_secs: int = 300) -> None:
    """Periodically regenerate CI status + main report so the dashboard stays fresh."""
    venv_py = os.path.join(project_root, '.venv', 'bin', 'python3')
    py = venv_py if os.path.exists(venv_py) else sys.executable
    ci_script = os.path.join(project_root, 'tools', 'report', 'generate_ci_status.py')
    report_script = os.path.join(project_root, 'tools', 'report', 'generate_decomp_report.py')
    report_json = os.path.join(serve_dir, 'report.json')
    report_html = os.path.join(serve_dir, 'index.html')

    while True:
        time.sleep(interval_secs)
        try:
            r = subprocess.run(
                [py, ci_script, '--output-dir', serve_dir],
                cwd=project_root, capture_output=True, timeout=30,
            )
            if r.returncode == 0:
                subprocess.run(
                    [py, report_script, '--output', report_json, '--html', report_html],
                    cwd=project_root, capture_output=True, timeout=60,
                )
                logging.info('Dashboard auto-refreshed')
            else:
                logging.warning('CI regen failed: %s', r.stderr.decode(errors='replace')[:200])
        except Exception as exc:
            logging.warning('Background regen error: %s', exc)


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
    logging.info('  Dashboard auto-refreshes every 5 min')

    regen_thread = threading.Thread(
        target=_background_regen,
        args=(serve_dir, project_root, 300),
        daemon=True,
    )
    regen_thread.start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        logging.info('Shutting down...')
        server.shutdown()


if __name__ == '__main__':
    main()
