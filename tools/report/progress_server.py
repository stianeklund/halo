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

# SSE client tracking, for visibility into how many browsers are connected
_sse_clients = 0
_sse_clients_mu = threading.Lock()

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

        logging.info('Score request for unit %s from %s', unit_name, self.client_address[0])

        # Per-unit lock so we don't run two objdiff instances for the same unit
        with _score_locks_mu:
            if unit_name not in _score_locks:
                _score_locks[unit_name] = threading.Lock()
            lock = _score_locks[unit_name]

        if lock.locked():
            logging.info('Unit %s is already scoring; waiting for it to finish', unit_name)

        with lock:
            result = self._run_score(unit_name)

        if result is None:
            logging.warning('Score request for unit %s failed (no_reference)', unit_name)
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

        report_unit = None
        for unit in report.get('units', []):
            if unit.get('name') == unit_name:
                report_unit = unit
                break
        if report_unit is None:
            logging.warning('Unit %s not found in report.json', unit_name)
            return None

        # Look up unit in objdiff.json to get the source file path and delinked ref.
        # Some delinked references are tracked only through delinked/manifest.json
        # and the generated report, so objdiff.json is a preferred source, not a
        # hard requirement.
        try:
            with open('objdiff.json') as f:
                objdiff_config = json.load(f)
        except Exception as e:
            logging.warning('Cannot read objdiff.json: %s', e)
            objdiff_config = {'units': []}

        unit_config = None
        for entry in objdiff_config.get('units', []):
            if entry['name'] == unit_name or entry['name'].endswith(f'/{unit_name}'):
                unit_config = entry
                break

        source_path_rel = None
        base_path = None
        if unit_config:
            source_path_rel = unit_config.get('metadata', {}).get('source_path')
            base_path = unit_config.get('base_path')
        if not source_path_rel:
            source_path_rel = report_unit.get('source_path')

        if not source_path_rel:
            logging.warning('No source_path in objdiff.json metadata for unit %s', unit_name)
            return None

        # Informational only.  Reference selection is vc71_verify's job (whole
        # delinked object -> per-function chunk -> reference synthesized from the
        # pristine XBE), resolved per function, so a missing delinked object here
        # is not a reason to refuse to score.
        if base_path and os.path.exists(base_path):
            logging.info('Unit %s has a whole-TU delinked reference: %s', unit_name, base_path)
        else:
            logging.info('Unit %s has no whole-TU delinked reference; vc71_verify '
                         'will resolve one per function', unit_name)

        # Score through vc71_regression's `populate`, scoped to this one TU.
        #
        # This handler used to call run_vc71_verify itself and then hand-roll the
        # raise-only floor merge.  Two problems that fix together:
        #   1. It was a SECOND writer of the score files, carrying its own copy
        #      of the floor policy.  The copies had already drifted once (see the
        #      make_score_entry docstring: provenance fields were being stripped
        #      on every dashboard refresh).
        #   2. It wrote ONLY the floor, while generate_decomp_report reads the
        #      honest current snapshot.  A button score was therefore erased by
        #      the next five-minute background regeneration.
        # `populate --source` writes floor + current + attention queue through
        # the one set of writers, so there is no policy left here to keep in sync.
        import sys as _sys
        from pathlib import Path
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.abspath(os.path.join(script_dir, '../..'))
        verify_dir = os.path.join(project_root, 'tools', 'verify')
        if verify_dir not in _sys.path:
            _sys.path.insert(0, verify_dir)
        try:
            import vc71_regression as _vc71
        except ImportError as e:
            logging.error('Cannot import vc71_regression: %s', e)
            return None

        source_path = Path(project_root) / source_path_rel
        if not source_path.exists():
            logging.warning('Source file not found for unit %s: %s', unit_name, source_path)
            return None

        logging.info('Running vc71 populate --source %s ...', source_path_rel)
        t_start = time.time()
        try:
            rc = _vc71.cmd_populate(argparse.Namespace(
                source=[str(source_path)],
                # Raise-only floor plus a merged honest snapshot — the same
                # policy the pre-commit gate applies.  NEVER --rebaseline from a
                # web endpoint: that REPLACES floors and can lower them.
                force=False, rebaseline=False, incremental=False,
                include_kb_only=True,
                # Costs one extra compile per function the whole-TU run missed.
                # Affordable for a single interactive unit, and it preserves the
                # per-function fallback this handler used to implement inline.
                per_function_fallback=True,
                workers=None, reset_journal=False, skip_decl_regen=False,
            ))
        except Exception as e:
            logging.error('populate failed for unit %s (%.1fs): %s',
                          unit_name, time.time() - t_start, e)
            return None
        verify_elapsed = time.time() - t_start
        if rc != 0:
            logging.warning('populate returned %d for unit %s (%.1fs) — '
                            'not a scoreable TU?', rc, unit_name, verify_elapsed)
            return None

        # Read back what populate persisted for THIS TU.  Mirroring from the
        # honest snapshot (rather than from a private in-memory result) is what
        # guarantees the numbers the button reports are the numbers the next
        # regeneration will render.
        def _same_source(value):
            return str(value or '').replace('\\', '/') == source_path_rel.replace('\\', '/')

        vc71_results = {fn: entry for fn, entry in _vc71.load_current().items()
                        if _same_source(entry.get('source'))}

        if not vc71_results:
            logging.warning('populate scored nothing for unit %s (%.1fs)',
                            unit_name, verify_elapsed)
            return None

        logging.info('populate finished for %s in %.1fs: %d function(s) scored',
                     source_path_rel, verify_elapsed, len(vc71_results))

        # Mirror the now-persisted scores into the live report.json so the open
        # dashboard updates immediately instead of waiting for the next
        # regeneration.  Join by name first, then by the address-keyed FUN_<addr>
        # alias (vc71_verify records some functions under their delinked
        # reference name) — identical to the generator's join, so the value shown
        # now and the value rendered after a regeneration are the same number.
        def _result_for(func):
            """Return the vc71_results entry for a function, or None.

            Same join order as the report generator (name, FUN_<addr> aliases,
            then namespace-suffix match) so a later regeneration reproduces it.
            """
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
                    return vc71_results[key]
            name = func.get('name')
            for key, info in vc71_results.items():
                if key.rsplit('::', 1)[-1] == name:
                    return info
            return None

        updated_funcs = {}
        updated_opnd = {}
        for unit in report.get('units', []):
            if unit['name'] == unit_name:
                for func in unit.get('functions', []):
                    info = _result_for(func)
                    if info is not None:
                        func['match_percent'] = round(info['score'], 2)
                        updated_funcs[func.get('name')] = func['match_percent']
                        # Advisory; tolerated absent on older cached lines.
                        opnd = info.get('opnd_percent')
                        if opnd is not None:
                            func['opnd_percent'] = round(opnd, 2)
                            updated_opnd[func.get('name')] = func['opnd_percent']
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

        total_elapsed = time.time() - t_start
        score_summary = ', '.join(
            f'{name}={score:.1f}%' for name, score in sorted(updated_funcs.items())
        )
        logging.info('VC71-scored unit %s: %d function(s) updated in %.1fs — %s',
                     unit_name, len(updated_funcs), total_elapsed, score_summary or 'none')
        return {'ok': True, 'unit': unit_name, 'scores': updated_funcs,
                'opnd_scores': updated_opnd}

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

        global _sse_clients
        with _sse_clients_mu:
            _sse_clients += 1
            active = _sse_clients
        logging.info('SSE client connected from %s (%d active)', self.client_address[0], active)

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
            with _sse_clients_mu:
                _sse_clients -= 1
                active = _sse_clients
            logging.info('SSE client disconnected from %s (%d active)', self.client_address[0], active)


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
        logging.info('Background dashboard refresh starting...')
        t_start = time.time()
        try:
            r = subprocess.run(
                [py, ci_script, '--output-dir', serve_dir],
                cwd=project_root, capture_output=True, timeout=30,
            )
            if r.returncode == 0:
                r2 = subprocess.run(
                    [py, report_script, '--output', report_json, '--html', report_html],
                    cwd=project_root, capture_output=True, timeout=60,
                )
                if r2.returncode == 0:
                    logging.info('Dashboard auto-refreshed in %.1fs', time.time() - t_start)
                else:
                    logging.warning('Report regen failed (rc=%d, %.1fs): %s',
                                     r2.returncode, time.time() - t_start,
                                     r2.stderr.decode(errors='replace')[:200])
            else:
                logging.warning('CI regen failed (rc=%d, %.1fs): %s',
                                 r.returncode, time.time() - t_start,
                                 r.stderr.decode(errors='replace')[:200])
        except Exception as exc:
            logging.warning('Background regen error (%.1fs): %s', time.time() - t_start, exc)


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
        # Loopback by default.  /api/score is an unauthenticated endpoint that
        # compiles source and rewrites tracked score files, so binding it to
        # every interface hands that to anyone on the network.  Pass an explicit
        # --host to expose it (and put access control in front of it first).
        '--host', default='127.0.0.1',
        help='Host to bind to (default: 127.0.0.1; /api/score is unauthenticated)'
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
