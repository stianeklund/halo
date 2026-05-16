#!/usr/bin/env python3
import sys, os
_tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

if __name__ == "__main__":
    from audit.check_requirements import check_requirements
    check_requirements()

import json
from collections import defaultdict
from typing import Sequence, Union, Optional
import logging
import os.path
import re
import argparse

import clang.cindex as clang


log = logging.getLogger(__name__)
root_dir = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../.."))
reg_filter_re = re.compile(r'@<(\w+)>')


def filter_reg_assignments(s: str) -> str:
	return reg_filter_re.sub('', s)

def strip_stdcall(s: str) -> str:
	return s.replace('__stdcall ', '')

_types_file: Optional[str] = None

def _get_types_file() -> str:
	global _types_file
	if _types_file is None:
		with open(os.path.join(root_dir, 'src', 'types.h')) as f:
			_types_file = f.read()
	return _types_file

def parse_string(s: str) -> str:
	s = _get_types_file() + filter_reg_assignments(s)
	index = clang.Index.create()
	tu = index.parse('tmp.h', args=['-target', 'i386-pc-win32'], unsaved_files=[('tmp.h', s)])
	return tu


_name_from_func_decl_re = re.compile(r'(\w+)\s*\(')
_name_from_data_decl_re = re.compile(r'[A-Za-z_]\w*')


def _extract_name_regex(decl: str, is_function: bool) -> Optional[str]:
	cleaned = reg_filter_re.sub('', decl)
	if is_function:
		m = _name_from_func_decl_re.search(cleaned)
		return m.group(1) if m else None
	else:
		tokens = _name_from_data_decl_re.findall(
			cleaned.rstrip(';').split('[')[0].split('=')[0]
		)
		return tokens[-1] if tokens else None


class Symbol:
	def __init__(self, decl: str, addr: Optional[Union[str, int]] = None, ported: Optional[bool] = None, **kwargs):
		self._parsed = None
		self._name_cache = None
		self.decl = decl
		if type(addr) is str:
			addr = int(addr, 16)
		self.addr = addr
		# Tri-state: None=field absent (treat as unported by tooling, but patch
		# semantics fall through to "is there a strong impl?" detection).
		# True=patch is active. False=skip the patch redirect at the original
		# address AND tail-call original from our impl entry, so all callers
		# (original and lifted) reach original code.
		self.ported = ported

	@property
	def cursor(self):
		if self._parsed is None:
			self._parsed = list(parse_string(self.decl).cursor.get_children())[-1]
		return self._parsed

	@property
	def name(self):
		if self._name_cache is None:
			self._name_cache = _extract_name_regex(self.decl, isinstance(self, Function))
		return self._name_cache

	def serialize(self):
		out = {'decl': self.decl, 'addr': hex(self.addr)}
		if self.ported is not None:
			out['ported'] = self.ported
		return out

	@property
	def requires_reg_thunk(self):
		return False

class Data(Symbol):
	def __repr__(self):
		return f'<Data "{self.decl}">'


class Function(Symbol):
	def __repr__(self):
		return f'<Function "{self.decl}">'

	@property
	def requires_reg_thunk(self):
		return reg_filter_re.search(self.decl) is not None

	@property
	def register_args(self):
		"""Return list of (param_index, reg_name) for every @<reg> annotation.

		Parses by walking the comma-separated parameter list and matching the
		@<reg> regex against each parameter in order. The result is ordered by
		parameter index, so [(0, 'ax'), (1, 'si')] means param 0 lives in AX
		and param 1 lives in SI."""
		open_paren = self.decl.find('(')
		close_paren = self.decl.rfind(')')
		if open_paren < 0 or close_paren < 0:
			return []
		params_src = self.decl[open_paren + 1:close_paren]
		depth = 0
		buf = []
		params = []
		for ch in params_src:
			if ch == '(' or ch == '<':
				depth += 1
				buf.append(ch)
			elif ch == ')' or ch == '>':
				depth -= 1
				buf.append(ch)
			elif ch == ',' and depth == 0:
				params.append(''.join(buf))
				buf = []
			else:
				buf.append(ch)
		if buf:
			params.append(''.join(buf))
		result = []
		for i, p in enumerate(params):
			m = reg_filter_re.search(p)
			if m is not None:
				result.append((i, m.group(1).lower()))
		return result

class KnowledgeBase:

	kb_path: str = os.path.join(root_dir, 'kb.json')

	def __init__(self):
		self.symbols = []
		self.object_to_symbols: Mapping[str, Sequence[Symbol]] = defaultdict(list)
		self.symbol_to_object: Mapping[Symbol, str] = {}
		self.object_to_source: Mapping[str, str] = {}
		self.name_to_addr: Mapping[str, int] = {}
		self.expected_md5: Optional[str] = None
		self.addr_to_symbols = {}

	def add_symbols(self, symbols: Sequence[Symbol]):
		self.symbols.extend(symbols)

	CALLEE_SAVED_REGS = {'ebx', 'esi', 'edi', 'ebp', 'bx', 'si', 'di'}
	REG_32 = {'eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp'}
	REG_16 = {'ax', 'bx', 'cx', 'dx', 'si', 'di', 'bp'}
	REG_8 = {'al', 'ah', 'bl', 'bh', 'cl', 'ch', 'dl', 'dh'}

	@staticmethod
	def _mov_for_reg(reg):
		"""Return the appropriate mov mnemonic for loading into this register."""
		if reg in KnowledgeBase.REG_32:
			return 'movl'
		elif reg in KnowledgeBase.REG_16:
			return 'movw'
		elif reg in KnowledgeBase.REG_8:
			return 'movb'
		return 'movl'

	@staticmethod
	def _push_reg(reg):
		"""Return push instruction for saving a register (always 32-bit on x86)."""
		if reg in KnowledgeBase.REG_32:
			return f'pushl %%{reg}'
		elif reg in KnowledgeBase.REG_16:
			# push the full 32-bit parent
			parent = {'ax': 'eax', 'bx': 'ebx', 'cx': 'ecx', 'dx': 'edx',
					  'si': 'esi', 'di': 'edi', 'bp': 'ebp'}[reg]
			return f'pushl %%{parent}'
		return f'pushl %%{reg}'

	@staticmethod
	def _pop_reg(reg):
		"""Return pop instruction for restoring a register."""
		if reg in KnowledgeBase.REG_32:
			return f'popl %%{reg}'
		elif reg in KnowledgeBase.REG_16:
			parent = {'ax': 'eax', 'bx': 'ebx', 'cx': 'ecx', 'dx': 'edx',
					  'si': 'esi', 'di': 'edi', 'bp': 'ebp'}[reg]
			return f'popl %%{parent}'
		return f'popl %%{reg}'

	def gen_thunk(self, s: Function):
		match = reg_filter_re.search(s.decl)
		assert match is not None

		reg_args = s.register_args
		rtype = s.cursor.result_type.spelling
		fname = s.cursor.spelling
		args = list(s.cursor.get_arguments())
		name = s.name
		mangled_name = s.cursor.mangled_name
		# __stdcall + register args: the thunk presents a cdecl interface
		# to the C caller, so strip the @N suffix from the mangled name.
		if '__stdcall' in s.decl:
			mangled_name = re.sub(r'@\d+$', '', mangled_name)
		name_alternate = name + '__thunk'
		mangled_name_alternate = mangled_name.replace(name, name_alternate)

		return self._gen_thunk_multi_reg(s, reg_args, rtype, fname, args, name, mangled_name, mangled_name_alternate)

	def _gen_thunk_multi_reg(self, s, reg_args, rtype, fname, args, name, mangled_name, mangled_name_alternate):
		"""Generate naked asm thunk for multi-register-arg functions.

		Emits a naked function that moves C-convention stack args into the
		required registers, pushes any remaining stack-only args, then calls
		the raw XBE address.  Callee-saved registers (EBX, ESI, EDI) are
		saved/restored; caller-saved (EAX, ECX, EDX) are not.
		"""
		reg_indices = {idx for idx, _ in reg_args}
		reg_map = {idx: reg for idx, reg in reg_args}
		stack_args = [(i, a) for i, a in enumerate(args) if i not in reg_indices]

		regs_used = [reg for _, reg in reg_args]
		callee_saved_used = [r for r in regs_used if r in self.CALLEE_SAVED_REGS]

		log.info('Generating multi-reg thunk for %s (regs: %s, stack: %d)',
				 fname, ', '.join(f'{r}' for r in regs_used), len(stack_args))

		# Build the __xbe pointer declaration (takes only stack args, or void)
		xbe_comment_parts = []
		for idx, reg in reg_args:
			xbe_comment_parts.append(f'{args[idx].type.spelling} {args[idx].spelling}@<{reg}>')
		xbe_comment = ', '.join(xbe_comment_parts)

		if stack_args:
			xbe_params = ', '.join(f'{args[i].type.spelling} {args[i].spelling}' for i, _ in stack_args)
		else:
			xbe_params = 'void'

		xbe_decl = f'{rtype} (*{fname}__xbe)(/* {xbe_comment}, */ {xbe_params})'

		# Build the naked thunk body in inline asm
		# Stack on entry: [ESP] = ret_addr, [ESP+4] = arg0, [ESP+8] = arg1, ...
		asm_lines = []
		num_saves = len(callee_saved_used)

		# Save callee-saved registers (always push/pop the 32-bit parent)
		for reg in callee_saved_used:
			asm_lines.append(self._push_reg(reg))

		# After saves, original [ESP+4+i*4] is now at [ESP+4+i*4+num_saves*4]
		save_offset = num_saves * 4

		# Push stack-only args in reverse order (rightmost first)
		num_stack_pushes = 0
		for i, _ in reversed(stack_args):
			offset = 4 + i * 4 + save_offset + num_stack_pushes * 4
			asm_lines.append(f'pushl {offset}(%%esp)')
			num_stack_pushes += 1

		# Load register args using size-appropriate mov
		extra_offset = save_offset + num_stack_pushes * 4
		for idx, reg in reg_args:
			offset = 4 + idx * 4 + extra_offset
			mov = self._mov_for_reg(reg)
			asm_lines.append(f'{mov} {offset}(%%esp), %%{reg}')

		# Call the raw XBE address
		asm_lines.append(f'call *%[fn]')

		# Clean up pushed stack args — but NOT if the original function is
		# __stdcall (callee-clean): the original's RET N already popped them.
		callee_cleans_stack = '__stdcall' in s.decl
		if num_stack_pushes > 0 and not callee_cleans_stack:
			asm_lines.append(f'addl ${num_stack_pushes * 4}, %%esp')

		# Restore callee-saved registers (reverse order)
		for reg in reversed(callee_saved_used):
			asm_lines.append(self._pop_reg(reg))

		asm_lines.append('ret')

		asm_body = '\\n\\t'.join(asm_lines)

		# Strip both @<reg> and __stdcall from the thunk declaration —
		# the thunk is always cdecl from the C caller's perspective.
		decl = strip_stdcall(filter_reg_assignments(s.decl))[:-1]

		thunk_functions = f'''\
#ifdef MSVC
#pragma comment(linker, "/alternatename:{mangled_name}={mangled_name_alternate}")
#endif

{xbe_decl} = (void*){s.addr:#x};

__attribute__((naked)) { decl.replace(name, 'THUNK('+name+')') }
{{
  __asm__ __volatile__("{asm_body}" : : [fn] "m"({fname}__xbe));
}}
'''
		return thunk_functions

	def build_header(self, path: str):
		log.info('Generating header...')
		with open(path, 'w') as f:
			f.write('//\n'
					'// AUTOMATICALLY GENERATED. DO NOT EDIT.\n'
					'//\n\n'
					'#define HFUNC __declspec(dllexport)\n'
					'#define HDATA __declspec(dllimport)\n'
					'\n')

			objs_sorted = sorted(n for n in self.object_to_symbols if n is not None)
			for object_name in (objs_sorted + [None]):
				symbols = self.object_to_symbols[object_name]
				f.write(f'// obj:{object_name or "?"} src:{self.object_to_source.get(object_name, "?")}\n')
				for s in sorted(symbols, key=lambda s: (isinstance(s, Function), s.name)):
					if isinstance(s, Data):
						f.write(f'HDATA {s.decl}\n')
					elif isinstance(s, Function):
						decl_str = filter_reg_assignments(s.decl)
						if s.requires_reg_thunk:
							decl_str = strip_stdcall(decl_str)
						f.write(f'HFUNC {decl_str}\n')
				f.write('\n')

			f.write('\n'
					'#undef HFUNC\n'
					'#undef HDATA\n'
					'\n'
					'//\n'
					'// AUTOMATICALLY GENERATED. DO NOT EDIT.\n'
					'//\n')

	def build_thunks(self, path: str):
		log.info('Generating thunks...')
		with open(path, 'w') as f:
			f.write("""
#ifdef MSVC
#pragma code_seg(".thunks")
#define asm __asm
#define THUNK(x) x ## __thunk
#else
#define THUNK(x) __attribute__((weak, section(".thunks"))) x
#endif

""")

			objs_sorted = sorted(n for n in self.object_to_symbols if n is not None)
			for object_name in (objs_sorted + [None]):
				symbols = self.object_to_symbols[object_name]
				for s in sorted(symbols, key=lambda s: (isinstance(s, Function), s.name)):
					if isinstance(s, Function) and s.requires_reg_thunk:
							t = self.gen_thunk(s)
							f.write(t)
							# FIXME: If we have an implementation, export
							#        the thunker to the patcher

	def build_def(self, path: str):
		log.info('Generating XBE export .def file...')

		with open(path, 'w') as f:
			f.write('LIBRARY halo.xbe\n'
					'EXPORTS\n')
			for s in sorted(self.symbols, key=lambda s: (isinstance(s, Function), s.name)):
				if s.requires_reg_thunk:
					continue
				export_name = s.name
				if isinstance(s, Function) and '__stdcall' in s.decl:
					# stdcall functions need _name@N decoration in the .def
					# so the linker can match the compiler-generated _name@N
					args = list(s.cursor.get_arguments())
					param_bytes = sum(
						max(a.type.get_size(), 4) for a in args
					)
					export_name = f'_{s.name}@{param_bytes}'
				elif isinstance(s, Function) and '__fastcall' in s.decl:
					# fastcall functions need @name@N decoration in the .def
					# so the linker can match the compiler-generated @name@N
					args = list(s.cursor.get_arguments())
					param_bytes = sum(
						max(a.type.get_size(), 4) for a in args
					)
					export_name = f'@{s.name}@{param_bytes}'
				f.write('\t' + export_name)
				if isinstance(s, Data):
					f.write(' DATA\n')
				else:
					f.write('\n')

	def serialize(self):
		log.info('Saving knowledge base to %s...', self.kb_path)

		with open(self.kb_path, 'w') as f:
			out = {
				'md5': self.expected_md5,
				'objects': []
			}

			# Make sure symbols are associated with an object (or the None object)
			for s in self.symbols:
				if s not in self.symbol_to_object:
					self.object_to_symbols[None].append(s)
					self.symbol_to_object[s] = None

			objs_sorted = sorted(n for n in self.object_to_symbols if n is not None)
			for obj in (objs_sorted + [None]):
				symbols = self.object_to_symbols[obj]
				if len(symbols) == 0:
					continue
				obj_data = {'name': obj}
				if obj in self.object_to_source:
					obj_data['source'] = self.object_to_source[obj]
				data = [s.serialize() for s in sorted(symbols, key=lambda s: s.addr) if isinstance(s, Data)]
				if data:
					obj_data['data'] = data
				funcs = [s.serialize() for s in sorted(symbols, key=lambda s: s.addr) if isinstance(s, Function)]
				if funcs:
					obj_data['functions'] = funcs
				out['objects'].append(obj_data)
			json.dump(out, f, indent=2)

	@classmethod
	def deserialize(cls) -> 'KnowledgeBase':
		log.info('Loading knowledge base...')
		with open(cls.kb_path) as f:
			serialized_kb = json.load(f)

		kb = KnowledgeBase()
		kb.expected_md5 = serialized_kb['md5']
		for o in serialized_kb['objects']:
			if 'source' in o:
				kb.object_to_source[o['name']] = o['source']
			obj_syms = []
			if 'functions' in o:
				obj_syms.extend([Function(**s) for s in o['functions']])
			if 'data' in o:
				obj_syms.extend([Data(**s) for s in o['data']])
			if obj_syms:
				kb.add_symbols(obj_syms)
				for s in obj_syms:
					kb.object_to_symbols[o['name']].append(s)
					kb.symbol_to_object[s] = o['name']
					if s.addr:
						kb.name_to_addr[s.name] = s.addr

		if os.path.exists(os.path.join(root_dir, 'tools', 'load_truth.py')):
			from load_truth import load_truth
			load_truth(kb)

		kb.addr_to_symbol = {s.addr:s for s in kb.symbols}

		num_symbols_with_truth = len([s for s in kb.symbols if kb.symbol_to_object[s]])
		num_symbols_without_truth = len(kb.symbols) - num_symbols_with_truth

		log.info('%d symbols were identified with objects, %d were not', num_symbols_with_truth, num_symbols_without_truth)
		num_objs_with_source = len([s for o, s in kb.object_to_source.items() if o in kb.object_to_symbols])
		log.info('%d of %d object files have known source mapping', num_objs_with_source, len(kb.object_to_symbols))

		# Check for duplicate function names across different addresses.
		# Duplicate names cause the linker to silently merge them, redirecting
		# one function's callers to a completely different implementation.
		func_names = {}
		for s in kb.symbols:
			if not isinstance(s, Function) or not s.addr:
				continue
			name = s.name
			if name in func_names and func_names[name] != s.addr:
				raise ValueError(
					f'Duplicate function name "{name}" at {hex(s.addr)} '
					f'and {hex(func_names[name])} — this will cause the '
					f'linker to silently merge them. Rename one.')
			func_names[name] = s.addr

		return kb


def main():
	ap = argparse.ArgumentParser()
	ap.add_argument('--gen-header', help='Generate import header')
	ap.add_argument('--gen-thunks', help='Generate import thunks')
	ap.add_argument('--gen-def', help='Generate import linker def file')
	ap.add_argument('--update', action='store_true', help='Re-serialize the KB')
	args = ap.parse_args()

	logging.basicConfig(level=logging.INFO)
	kb = KnowledgeBase.deserialize()

	if args.gen_header:
		kb.build_header(args.gen_header)

	if args.gen_thunks:
		kb.build_thunks(args.gen_thunks)

	if args.gen_def:
		kb.build_def(args.gen_def)

	if args.update:
		kb.serialize()


if __name__ == '__main__':
	main()
