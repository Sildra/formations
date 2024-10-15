import argparse
import sys
import subprocess
import re
import os
import json
from collections import defaultdict

class Bunch(object):
    pass

class BunchEncoder(json.JSONEncoder):
    def default(self, o):
        return o.__dict__


def dump_stack_trace(pid, filename=None):
    print('Dumping stack trace of ' + pid + ((' into ' + filename) if filename else ''))
    result = subprocess.run(['pstack', pid], stdout=subprocess.PIPE, universal_newlines=True)
    if filename:
        with open(filename, 'w') as file:
            file.write(result.stdout)
    return result.stdout.split('\n')


def read_stack_trace(filename):
    print(f'Reading stack trace from {filename}')
    with open(filename, 'r') as file:
        return file.readlines()


def analyze_functions(raw_functions):
    functions = Bunch()
    functions.functions = defaultdict(dict)
    functions.paths = defaultdict(dict)
    functions.path_stripper = defaultdict(dict)
    current_functions = []
    for r in raw_functions:
        f = Bunch()
        f.path = r[:r.find(':')]
        f.basename = os.path.basename(f.path)
        f.start_line = int(r[len(f.path) + 1:r.find(' ')])
        f.function = r[r.find(' ') + 1:].strip()
        if len(current_functions) != 0 and (current_functions[0].path != f.path or current_functions[0].start_line != f.start_line):
            end_line = f.start_line if current_functions[0].path == f.path else 9999999
            for c in current_functions:
                c.end_line = end_line
                functions.paths[c.basename].setdefault(c.path, [])
                functions.paths[c.basename][c.path].append(c)
                functions.functions[c.function].setdefault(c.path, [])
                functions.functions[c.function][c.path].append(c)
            current_functions = []
        current_functions.append(f)

    return functions


def dump_functions_addr(binary, filename):
    print('Dumping functions with addr2line from ' + binary + ((' into ' + filename) if filename else ''))
    result = subprocess.run(['objdump', '-d', binary], stdout=subprocess.PIPE, universal_newlines=True)
    addr2line_input = []
    for s in result.stdout.split('\n'):
        s = s.strip()
        if len(s) == 0 or (s[0] != 0 and s[-1] != '>'):
            continue
        address = s[:s.find(' ')].strip()
        addr2line_input.append(address)
    addr2line_input = list(set(addr2line_input))
    
    print(f'Analyzing {len(addr2line_input)} functions')
    addr2line_input = '\n'.join(addr2line_input)
    result = subprocess.run(['addr2line', '-C', '-f', '-p', '-e', binary], input=addr2line_input, stdout=subprocess.PIPE, universal_newlines=True)
    functions = []
    for s in result.stdout.split('\n'):
        if len(s) == 0 or s[-1] == '?': continue
        match = re.fullmatch(r'(.*) at (.*):(\d+)', s)
        if match:
            function = match.group(1)
            ##function = function[:function.rfind('(')]
            ##function = function[function.rfind(')') + 1:].strip()
            functions.append(f'{match.group(2)}:{match.group(3).zfill(8)} {function}')

    functions = sorted(set(functions))
    if filename:
        with open(filename, 'w') as file:
            for f in functions:
                file.write(f)
                file.write('\n')
    return analyze_functions(functions)


def read_functions(filename):
    print(f'Reading functions from {filename}')
    with open(filename, 'r') as file:
        return analyze_functions(file.readlines())


def locate_function(functions, function_name, path, line):
    result = { 'function' : function_name, 'path': path, 'start_line': line, 'line_number': line, 'inline_call': None, 'error': False }
    if not functions:
        return result
    basename = os.path.basename(path)
    line = int(line)
    try:
        rpath = path[::-1]
        common_path = ''
        max_f = None
        # Lookup best definition file based on the common reverse path between function definition and stack trace
        for p, f in functions.paths[basename].items():
            rp = p[::-1]
            common = os.path.commonpath([rp, rpath])
            if len(common) > len(common_path):
                common_path = common
                max_f = f
        common_path = common_path[::-1]
        candidates_f = []
        for f in max_f:
            if line >= f.start_line and line < f.end_line:
                candidates_f.append(f)
        for candidate in candidates_f:
            if candidate.function == function_name:
                functions.path_stripper[candidate.path[:-len(common_path)]] = path[:-len(common_path)]
                result['start_line'] = candidate.start_line
                return result
        if len(candidates_f) != 1: raise IndexError(f'No unique candidate for {function_name}, {len(candidates_f)} candidates')
        candidate_function = candidates_f[0].function
        o = functions.functions[function_name]
        if len(o) != 1: raise IndexError(f'No unique path for {function_name}, {len(o)} paths found')
        (p, o) = list(o.items())[0]
        if len(o) != 1: raise IndexError(f'No unique block for {function_name}, {p}:[{", ".join([str(v.start_line) + ":" + str(v.end_line) for v in o])}]')
        o = o[0]
        result['path'] = o.path
        result['start_line'] = o.start_line
        result['line_number'] = o.end_line
        result['inline_call'] = candidate_function[candidate_function.rfind(':') + 1:]

    except Exception as e:
        result['error'] = True
        print('\033[31m[Error]\033[0m', e)
    return result


def analyze_stack(stack_trace, functions, filters):
    stack_data = defaultdict(dict)
    thread_id = '1'
    for line in stack_trace:
        # Thread line
        match = re.match(r'Thread (\d+)', line)
        if match:
            thread_id = match.group(1)
            continue
        # Stack line
        match = re.match(r'(#\d+) .* in (.+) at (.+):(\d+)', line)
        if not match: match = re.match(r'(#\d+) (.+) at (.+):(\d+)', line)
        if not match: continue
        stack_depth, function_name, path, line_number = match.groups()
        filtered = False
        for match_filter in filters:
            if re.match(match_filter, path):
                filtered = True
                break
        if filtered: continue
        function_name = function_name[0:function_name.find('(')]
        function_name = function_name[function_name.find(')') + 1:].strip()
        mapped_function = locate_function(functions, function_name, path, line_number)
        path = mapped_function['path']

        tokens = [it.start() for it in re.finditer(r':', function_name)]
        if len(tokens) > 3:
            function_name = function_name[tokens[-3] + 1:]
        function_name = function_name.strip()
        if function_name == 'operator': continue
        stack_key = line_number.zfill(8) + '#' + function_name
        stack_data[path].setdefault(stack_key, {
            'mapped_function': mapped_function,
            'source': 'TBD',
            'locks': [],
            'threads': [],
        })
        stack_data[path][stack_key]['threads'].append(thread_id + stack_depth)
    '''
    if functions:
        mapped_paths = dict
        for path in stack_data:
            for s, d in functions.path_stripper.items():
                if path.startswith(s):
                    pass
    '''

    analyze_locks(stack_data)
    return stack_data


def analyze_locks(stack_data):
    errors = []
    sources = []
    for path, lines in stack_data.items():
        source = None
        try:
            with open(path, 'r') as file:
                source = strip_source_file(file.readlines())
                sources.append(path)
        except FileNotFoundError:
            for line_number, lock in lines.items():
                lock['locks'].append(extract_lock_info('', '!ERROR!'))
            errors.append(path)
            continue

        to_pop = []
        for stack_key, lock in lines.items():
            mapped_function = lock['mapped_function']
            line_number = int(mapped_function['line_number'])
            filtered_source, lock_lines, lock_state = find_function_lines(mapped_function['function'], source, line_number)
            if len(lock_lines) == 0:
                to_pop.append(line_number)
                continue
            for lock_line in lock_lines:
                lock['locks'].append(extract_lock_info(lock_line, lock_state))
                lock_state = 'LOCKING'
            lock['source'] = filtered_source
            lock['locks'].reverse()
        for pop in to_pop:
            lines.pop(pop, None)

    for filename in errors:
        stack_data['[error] ' + filename] = stack_data.pop(filename)
    for filename in sources:
        stack_data['[source] ' + filename] = stack_data.pop(filename)


def strip_source_file(source):
    in_multiline_comment = False
    for i, s in enumerate(source):
        if in_multiline_comment:
            match_end_multi = re.search(r'\*/', s)
            if not match_end_multi:
                source[i] = ''
                continue
            in_multiline_comment = False
            s = s[match_end_multi.end():].rstrip()
        while True:
            match_single = re.search(r'//', s)
            match_multi = re.search(r'/\*', s)
            if match_single and (not match_multi or match_single.start() < match_multi.start()):
                s = s[0:match_single.start()]
                break
            elif match_multi:
                match_end_multi = re.search(r'\*/', s)
                if match_end_multi:
                    s = s[0:match_multi.start()] + s[match_end_multi.end():]
                    match_multi = re.search(r'/\*', s)
                else:
                    s = s[0:match_multi.start()]
                    in_multiline_comment = True
                    break
            else:
                break

        source[i] = s.rstrip()
    return source

def strip_scopes(source, current_line):
    stripped_code = []

    current_braces = 0
    inside_block = False
    for i in range(current_line, -1, -1):
        line = source[i]
        open_braces = line.count('{')
        close_braces = line.count('}')
        current_braces += open_braces - close_braces
        if current_braces > 0:
            current_braces = 0
        if current_braces == 0 and not inside_block:
            stripped_code.insert(0, line)
        elif close_braces > 0:
            stripped_code.insert(0, '')
            current_braces -= 1
            inside_block = True
        elif open_braces > 0:
            current_braces += 1
            if current_braces == 0:
                inside_block = False
                occ = min(line.rfind('}'), line.rfind('{'), line.rfind(';'))
                line = line[:occ + 1]
                stripped_code.insert(0, line)
            else:
                stripped_code.insert(0, '')
                inside_block = True
        else:
            stripped_code.insert(0, '')
    return stripped_code


def find_function_lines(function_name, source, current_line):
    filtered_source = []
    locks = []
    current_line = current_line - 1
    state = 'LOCKING'
    if re.search(r'std::\w*lock\w*', source[current_line]): state = 'BLOCKED'
    if '.wait(' in source[current_line]: state = 'WAITING'
    if '.wait_for(' in source[current_line]: state = 'WAITING'
    append_to_source = (state == 'LOCKING')
    source = strip_scopes(source, current_line)
    while current_line > 0:
        current_source = source[current_line]
        current_line = current_line - 1
        if re.search(r'std::\w*lock\w*', current_source):
            locks.append(str(current_line + 2) + ' ' + current_source)
            append_to_source = True
        if append_to_source:
            filtered_source.append(current_source)
        if function_name in current_source:
            break
    filtered_source.reverse()
    return filtered_source, locks, state


def extract_lock_info(line, state):
    lock_line = '???'
    lock_type = '???'
    lock_name = '???'
    mutex = lock_line
    match = re.match(r'(?P<lock_line>\d+) .*(?P<lock_type>std::\w*lock\w*)(<[^>]*>)?\W*(?P<lock_name>\w*)\W*[\({]\W*(?P<mutex>\w*)\W*[})]', line)
    if match:
        lock_line = match.group('lock_line')
        lock_type = match.group('lock_type')
        lock_name = match.group('lock_name')
        mutex = match.group('mutex')
    elif len(line) != 0:
        print('\033[92mWARNING\033[0m Unable to parse lock line ' + line)
    return {
        'lock_line': lock_line,
        'lock_type': lock_type,
        'lock_name': lock_name,
        'mutex': mutex,
        'state': state
    }


def display_stack(stack_data, display_code):
    threads = defaultdict(dict)
    header = ['State', 'Function', 'Lock Type', 'Lock Name', 'Mutex', 'Threads', 'Path', 'Line Number']
    print('\nLocks:')
    print('|'.join(header))
    for path, lines in sorted(stack_data.items()):
        for stack_key, locks in sorted(lines.items()):
            thread_str = ', '.join(locks['threads'])
            for lock_info in locks['locks']:
                state_color = '\033[' + { '!ERROR!': '91', 'BLOCKED': '91', 'LOCKING': '93', 'INLINED': '95' }.get(lock_info['state'], '92') + 'm'
                state = state_color + lock_info['state'] + '\033[0m'
                function = locks['mapped_function']['function'] + ':' + lock_info['lock_line']
                data = [
                    state,
                    function,
                    lock_info['lock_type'],
                    lock_info['lock_name'],
                    lock_info['mutex'],
                    f'[{thread_str}]' if thread_str else '',
                    path,
                    locks['mapped_function']['line_number']
                ]
                lock_line = lock_info['lock_line']
                try: lock_line = int(lock_line)
                except Exception: lock_line = 0
                for thread in locks['threads']:
                    thread_id, thread_stack = thread.split('#')
                    # lock_line
                    thread_stack_key = thread_stack.zfill(8) + str(99999999 - lock_line).zfill(8)
                    threads[int(thread_id)][thread_stack_key] = '#' + thread_stack.ljust(3) + state + ': ' + function + '.' + lock_info['mutex']
                print('|'.join(map(str, data)))
                if display_code:
                    if locks['source']: locks['source'][-1] = locks['source'][-1] + ' \033[35m<- we are here\033[0m'
                    print(*locks['source'], sep='\n')
    merged_threads = defaultdict(dict)
    for thread_id, stack in sorted(threads.items()):
        stack_string = '        ' + '\n     -> '.join([info for stack__id, info in sorted(stack.items(), reverse=True)])
        merged_threads.setdefault(stack_string, [])
        merged_threads[stack_string].append(str(thread_id))
    print('\nThreads:')
    for stack_string, thread_ids in merged_threads.items():
        print('Threads: ' + ', '.join(thread_ids))
        print(stack_string)


def parse_arguments():
    parser = argparse.ArgumentParser(description='A C++ deadlock finder')
    sub_stack = parser.add_argument_group('Stack Trace  arguments')
    sub_stack.add_argument('--st', action='store', help='Stack trace file, created if --pid is present')
    sub_stack.add_argument('--pid', action='store', help='Generate a stack trace with pstack on this program')
    sub_stack.add_argument('--filter', action='append', help='Filter a pattern from the analysis', default=[])
    
    sub_source = parser.add_argument_group('Source arguments')
    sub_source.add_argument('--cd', action='store', help='Compilation directory', default='.')
    sub_source.add_argument('--display_code', action='store_true', help='Display the code since related to each lock')

    sub_bin = parser.add_argument_group('Binaries argument')
    sub_bin.add_argument('--bin', action='store', help='Binary used for optional analysis')
    sub_bin_gen = sub_bin.add_mutually_exclusive_group(required=False)
    sub_bin_gen.add_argument('--addr', action='store_true', help='Generate function information with addr2line')
    sub_bin.add_argument('--functions', action='store', help='Function information file')

    args = parser.parse_args()
    if not args.st or args.pid:
        print('error: argument --st or --pid must be present')
        exit(1)
    if args.addr and not args.bin:
        print('error: argument --addr requires --bin')
        exit(1)

    return args


def main():
    args = parse_arguments()
    if args.pid:
        stack_trace = dump_stack_trace(args.pid, args.st)
    else:
        stack_trace = read_stack_trace(args.st)

    functions = None
    if args.bin:
        if args.addr:
            functions = dump_functions_addr(args.bin, args.functions)
    elif args.functions:
        functions = read_functions(args.functions)

    os.chdir(args.cd)
    stack_data = analyze_stack(stack_trace, functions, args.filter)
    display_stack(stack_data, args.display_code)


if __name__ == "__main__":
    main()