import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

from rpc_client import GhidraLiveClient


client = GhidraLiveClient()


def get_current_program():
    return client.call("get_current_program")


def get_current_selection():
    return client.call("get_current_selection")


def run_relocation_synthesizer(selection_mode, range=None):
    return client.call(
        "run_relocation_synthesizer",
        {
            "selection_mode": selection_mode,
            "range": range,
        },
    )


def export_delinked_object(
    export_path,
    exporter_name="COFF relocatable object",
    selection_mode="current_selection",
    range=None,
    run_relocation_synthesizer=True,
):
    return client.call(
        "export_delinked_object",
        {
            "export_path": export_path,
            "exporter_name": exporter_name,
            "selection_mode": selection_mode,
            "range": range,
            "run_relocation_synthesizer": run_relocation_synthesizer,
        },
    )


def list_symbols_in_range(range):
    return client.call("list_symbols_in_range", {"range": range})


def get_last_export_status():
    return client.call("get_last_export_status")
