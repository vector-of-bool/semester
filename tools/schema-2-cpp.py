import sys
import json
import argparse
import string
from pathlib import Path
from typing import Sequence, List, Dict, Any
import textwrap

SchemaContent = Dict[str, Any]


def cpp_safe_name(n: str) -> str:
    return n.replace('-', '_')


class CppProperty:
    def __init__(self, name: str, typename: str, required: bool) -> None:
        self.name = name
        self.typename = typename
        self.required = required

    def generate_cpp(self) -> str:
        tn = cpp_safe_name(self.typename)
        if not self.required:
            tn = f'std::optional<{tn}>'
        n = cpp_safe_name(self.name)
        return f'{tn} {n}'


class CppObject:
    def __init__(self, name: str, props: Sequence[CppProperty]) -> None:
        self.name = name
        self.props = props

    def generate_cpp(self) -> str:
        lines = [f'class {self.name} {{']
        for prop in self.props:
            lines.append('    ' + prop.generate_cpp() + ';')

        lines.append('};\n')
        return lines


class Schema:
    def __init__(self) -> None:
        self._object_types: List[CppObject] = []

    def _recurse(self, schema: SchemaContent) -> None:
        typ = schema['type']
        if typ == 'object':
            self._add_object(schema, 'root')

    def _add_object(self, schema: SchemaContent, typename: str) -> CppObject:
        all_props = schema.get('properties', {})
        req_props = list(schema.get('required', []))
        props = (self._load_prop(key, defn, required=key in req_props)
                 for key, defn in all_props.items())
        obj = CppObject(typename, list(props))
        self._object_types.append(obj)
        return obj

    def _load_prop(self, key: str, prop: SchemaContent,
                   required: bool) -> CppProperty:
        typ = prop['type']
        if typ == 'number':
            return CppProperty(key, 'double', required)

        if typ == 'string':
            return CppProperty(key, 'std::string', required)

        typename = f'_{key}_type'

        if typ == 'array':
            item_prop = self._add_object(prop['items'], typename)
            return CppProperty(
                key, f'std::vector<{item_prop.name}>', required=True)

        assert typ == 'object', f'Not-yet implemented schema type {typ}'

        if 'additionalProperties' not in prop:
            # A simple single property
            self._add_object(prop, typename)
            return CppProperty(key, typename, required)

        # This one is a map of properties
        addnl = prop['additionalProperties']
        if 'object' in addnl:
            item_type = self._add_object(prop, typename)
            subtype_name = item_type.name
        else:
            subtype_name = self._load_prop(key, addnl, True).typename

        return CppProperty(
            key, f'std::map<std::string, {subtype_name}>', required=True)

    @staticmethod
    def build(schema: SchemaContent, root_name: str) -> 'Schema':
        assert isinstance(schema, dict), 'Root should be an object'
        inst = Schema()
        inst._add_object(schema, root_name)
        return inst

    def write_cpp(self, out: Path, ns: str) -> None:
        lines = [
            '/**',
            ' * This file is GENERATED! DO NOT EDIT!',
            ' */',
            '#pragma once',
            '',
            '#include <semester/json.hpp>',
            '',
            '#include <optional>',
            '#include <variant>',
            '',
            f'namespace {ns} {{',
            '',
        ]
        for cls in self._object_types:
            lines.extend(cls.generate_cpp())
        lines.extend([f'}} // namespace {ns}', ''])
        content = '\n'.join(lines)
        out.write_text(textwrap.dedent(content))


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('schema', help='Path to a json schema file')
    parser.add_argument(
        '--namespace', help='C++ namespace prefix', required=True)
    parser.add_argument('--out', help='The file to write', required=True)
    parser.add_argument(
        '--root-typename', help='The name of the root type', required=True)
    args = parser.parse_args(argv)

    schema_dat = json.loads(Path(args.schema).read_text())

    schema = Schema.build(schema_dat, args.root_typename)
    schema.write_cpp(Path(args.out), args.namespace)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
