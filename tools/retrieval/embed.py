"""jina-embeddings-v2-base-code embedder for the retrieval index.

Loads the model lazily (~322MB on first use, cached under
~/.cache/huggingface/) and produces 768-dim float32 embeddings for the
two text signals stored in the index: pseudocode and c_source.

Mizuchi-style retrieval embeds both at INDEX time. At QUERY time the
agent embeds the new target's Ghidra pseudocode and searches both
columns — same-modal (pseudocode↔pseudocode) is the primary signal,
cross-modal (pseudocode↔c_source) is the fallback when no neighbor has
cached pseudocode.

Truncation: jina-v2-code accepts 8K tokens. Functions exceeding that
are truncated; the embedding still captures the body's overall shape.
"""

from __future__ import annotations

import logging
import sys
from pathlib import Path
from typing import Iterable, Optional

# venv shim
_REPO_ROOT = Path(__file__).resolve().parent.parent.parent
_VENV_SP = _REPO_ROOT / ".venv" / "lib" / "python3.12" / "site-packages"
if _VENV_SP.exists() and str(_VENV_SP) not in sys.path:
    sys.path.insert(0, str(_VENV_SP))

MODEL_NAME = "jinaai/jina-embeddings-v2-base-code"
EMBED_DIM = 768

_log = logging.getLogger("retrieval.embed")


class Embedder:
    """Cached singleton wrapper around the SentenceTransformer model."""

    _model = None  # class-level cache so repeat instantiations are free

    def __init__(self, model_name: str = MODEL_NAME):
        self.model_name = model_name

    @property
    def model(self):
        if Embedder._model is None:
            from sentence_transformers import SentenceTransformer
            device = "cpu"
            print(f"[embed] loading {self.model_name} on {device}", flush=True)
            Embedder._model = SentenceTransformer(
                self.model_name,
                trust_remote_code=True,  # jina-v2 ships custom modeling code
                device=device,
            )
        return Embedder._model

    def embed_one(self, text: Optional[str]) -> Optional[list[float]]:
        if not text:
            return None
        vec = self.model.encode(
            text,
            convert_to_numpy=True,
            normalize_embeddings=True,  # so cosine == dot product
            show_progress_bar=False,
        )
        return [float(x) for x in vec.tolist()]

    def embed_batch(
        self,
        texts: list[Optional[str]],
        *,
        batch_size: int = 4,
    ) -> list[Optional[list[float]]]:
        """Embed a list of texts; preserves None entries."""
        # Hard cap on characters as a fast pre-filter; the tokenizer enforces
        # the real token limit below so dense single-char inputs can't exceed
        # MAX_TOKENS regardless of character count.
        MAX_CHARS = 6_000
        MAX_TOKENS = 1024  # attention is O(n²): 1024 tokens ≈ 50 MB/layer
        idx_text = [(i, t[:MAX_CHARS]) for i, t in enumerate(texts) if t]
        if not idx_text:
            return [None] * len(texts)
        keep_texts = [t for _, t in idx_text]
        old_max = getattr(self.model, "max_seq_length", None)
        if old_max is None or old_max > MAX_TOKENS:
            self.model.max_seq_length = MAX_TOKENS
        vecs = self.model.encode(
            keep_texts,
            convert_to_numpy=True,
            normalize_embeddings=True,
            batch_size=batch_size,
            show_progress_bar=False,
        )
        if old_max is not None:
            self.model.max_seq_length = old_max
        out: list[Optional[list[float]]] = [None] * len(texts)
        for (orig_idx, _), v in zip(idx_text, vecs):
            out[orig_idx] = [float(x) for x in v.tolist()]
        return out
