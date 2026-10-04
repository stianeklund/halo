"""Tests for splice_compile.body_span (no VC71 needed)."""

import importlib.util
from pathlib import Path

import pytest

_spec = importlib.util.spec_from_file_location(
    "splice_compile", Path(__file__).with_name("splice_compile.py"))

try:
    splice_compile = importlib.util.module_from_spec(_spec)
    _spec.loader.exec_module(splice_compile)
except Exception as exc:  # pragma: no cover - verify modules unavailable
    pytest.skip("splice_compile import failed: %s" % exc, allow_module_level=True)


def _body(text, func):
    span = splice_compile.body_span(text, func)
    return None if span is None else text[span[0]:span[1]]


def test_skips_prototype_and_calls():
    text = ("void f(int a);\n"
            "void g(void) { f(1); }\n"
            "void f(int a)\n{\n  a++;\n}\n")
    assert _body(text, "f") == "{\n  a++;\n}"


def test_ignores_braces_in_comments_and_strings():
    text = ("/* f(x) { */\n"
            "int f(int x) /* } */\n"
            "{\n  const char *s = \"}{\";\n  char c = '}';\n  // }\n  return x;\n}\n")
    assert _body(text, "f").endswith("return x;\n}")


def test_name_prefix_does_not_match():
    text = "void ff(void) { }\nvoid f(void) { int y; }\n"
    assert _body(text, "f") == "{ int y; }"


def test_missing_function():
    assert splice_compile.body_span("void g(void) {}\n", "f") is None
