import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

from pathlib import Path
import importlib.metadata
import logging
import re

from internal import color

log = logging.getLogger(__name__)


def check_requirements() -> None:
	requirements_path = Path(__file__).resolve().parents[2] / "requirements.txt"

	with open(requirements_path, "r", encoding="utf-8") as f:
		lines = [l.strip() for l in f if l.strip() and not l.strip().startswith('#')]

	for req_str in lines:
		m = re.match(r'^([A-Za-z0-9_.-]+)', req_str)
		if not m:
			continue
		pkg_name = m.group(1)
		try:
			importlib.metadata.version(pkg_name)
		except importlib.metadata.PackageNotFoundError:
			log.error("Required package '%s' is not installed. Please install project requirements via requirements.txt.", req_str)
			exit(1)


def main():
	check_requirements()


if __name__ == '__main__':
    import os
    logging.basicConfig(level=getattr(logging, os.environ.get('LOG_LEVEL', 'INFO').upper(), logging.INFO), handlers=[color.ColorLogHandler()])
    main()
