import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

import urllib.error
import urllib.parse
import urllib.request


class GhidraLiveRpcError(RuntimeError):
    pass


class GhidraLiveClient:
    def __init__(self, url="http://127.0.0.1:18080/rpc"):
        self.url = url

    def call(self, method, params=None):
        form = {"method": method}
        if params:
            for key, value in params.items():
                if value is None:
                    continue
                form[key] = str(value)

        payload = urllib.parse.urlencode(form).encode("utf-8")

        request = urllib.request.Request(
            self.url,
            data=payload,
            headers={"Content-Type": "application/x-www-form-urlencoded"},
            method="POST",
        )

        try:
            with urllib.request.urlopen(request, timeout=60) as response:
                data = self._parse_response(response.read().decode("utf-8"))
        except urllib.error.URLError as exc:
            raise GhidraLiveRpcError(f"failed to contact live Ghidra RPC at {self.url}: {exc}")

        if not data.get("ok"):
            error = data.get("error", {})
            code = error.get("code", "UNKNOWN")
            message = error.get("message", "unknown error")
            raise GhidraLiveRpcError(f"{code}: {message}")

        return data.get("result")

    def _parse_response(self, text):
        lines = text.splitlines()
        if not lines:
            raise GhidraLiveRpcError("empty response from live Ghidra RPC")

        status = lines[0].strip()
        payload = {}
        for line in lines[1:]:
            if not line or "=" not in line:
                continue
            key, value = line.split("=", 1)
            payload[key] = value.replace("\\n", "\n").replace("\\\\", "\\")

        if status == "ok":
            return {"ok": True, "result": payload}

        return {
            "ok": False,
            "error": {
                "code": payload.get("code", "UNKNOWN"),
                "message": payload.get("message", "unknown error"),
            },
        }
