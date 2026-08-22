"""Capture-profile names and their default pool selections.

Full Fidelity keeps the legacy trajectory capture pool set.  AI Core and AI
Perception are deliberately narrower; callers can still pass an explicit pool
tuple for legacy/custom capture commands.
"""

PROFILES = ("full-fidelity", "ai-core", "ai-perception")

_ALIASES = {
    "full": "full-fidelity",
    "fullfidelity": "full-fidelity",
    "full-fidelity": "full-fidelity",
    "aicore": "ai-core",
    "ai-core": "ai-core",
    "aiperception": "ai-perception",
    "ai-perception": "ai-perception",
}

PROFILE_POOLS = {
    "full-fidelity": ("objects", "players", "actors", "props"),
    "ai-core": ("objects", "players", "actors"),
    "ai-perception": ("objects", "players", "actors", "props"),
}


def normalize_profile(value):
    """Return the canonical profile name or raise ValueError."""
    key = str(value).strip().lower().replace("_", "-")
    try:
        return _ALIASES[key]
    except KeyError:
        raise ValueError("unknown capture profile %r (choose one of %s)" %
                         (value, ", ".join(PROFILES)))


def pools_for_profile(profile, pools=None):
    """Return explicit pools when supplied, otherwise profile defaults."""
    canonical = normalize_profile(profile)
    if pools is not None:
        return tuple(pools)
    return PROFILE_POOLS[canonical]
