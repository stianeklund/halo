import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

from pathlib import Path
import logging

import pkg_resources

from internal import color

log = logging.getLogger(__name__)


def check_requirements() -> None:
	requirements_path = Path(__file__).resolve().parents[2] / "requirements.txt"

	with open(requirements_path, "r", encoding="utf-8") as f:
		requirements = f.readlines()

	try:
		pkg_resources.require(requirements)
	except pkg_resources.DistributionNotFound as e:
		log.error("Required package '%s' is not installed. Please install project requirements via requirements.txt.", e.req)
		exit(1)
	except pkg_resources.VersionConflict as e:
		log.error("There is a package version conflict. '%s' is installed, but '%s' is required. Please install project requirements via requirements.txt.", e.dist, e.req)
		exit(1)


def main():
	check_requirements()


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO, handlers=[color.ColorLogHandler()])
    main()
