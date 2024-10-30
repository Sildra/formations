import os
import re
import sys
import subprocess
import argparse

def import_snippets(sourcepath, snippets):
    # Load comment snippets
    with open(sourcepath, 'r') as source:
        current_token = None
        for line in source:
            comment = re.match(r'(.*)\s*\/\/\s*(\S+)$', line)
            if comment:
                line = comment.group(1).strip()
                current_token = comment.group(2).strip()
                snippets[current_token] = []
                if len(line) == 0:
                    continue
            if current_token:
                snippets[current_token].append(line.rstrip())
    # Desindent code blocks
    for value in snippets.values():
        if not len(value): continue
        leading_spaces = len(value[0]) - len(value[0].lstrip())
        for line in value[1:]:
            if len(line):
                leading_spaces = min(leading_spaces, len(line) - len(line.lstrip()))
        for line in range(len(value)):
            if len(value[line]):
                value[line] = value[line][leading_spaces:]
        if not len(value[-1].strip()):
            value.pop()


def execute(dirname, command, output):
    print(f'  Executing {command}')
    out = subprocess.run(os.path.expandvars(command), cwd=dirname, capture_output=True, shell=True, universal_newlines=True)
    output.write(str(out.stdout).expandtabs())
    if out.stderr:
        print(re.sub('^', ' ' * 12, out.stderr, flags=re.MULTILINE), end=None)


def process_dir(dirname):
    template = os.path.join(dirname, 'template.md')
    if not os.path.isfile(template):
        return
    print(f'Creating {dirname}')
    snippets = {}
    for sourcename in os.listdir(dirname):
        if sourcename.endswith(('.cpp', '.c', '.hpp', '.h')):
            import_snippets(os.path.join(dirname, sourcename), snippets)
    os.makedirs('formations', exist_ok=True)
    with open(os.path.join('formations', dirname) + '.md', 'w') as markdown, open(template, 'r') as template:
        markdown.write('[Sommaire](../README.md)\n\n')
        for line in template:
            comment = re.match(r'(\s*)\/\/\s*(\S+)$', line)
            if comment:
                space = comment.group(1)
                for snip in snippets[comment.group(2)]:
                    markdown.write(space + snip + '\n')
                continue
            executor = re.match(r'^>!? (.*)$', line)
            if executor:
                if line[1] != '!':
                    markdown.write(line)
                execute(dirname, executor.group(1), markdown)
                continue
            markdown.write(line)
    for execname in os.listdir(dirname):
        if execname.endswith('.exe'):
            os.remove(os.path.join(dirname, execname))


def _parse_args():
    parser = argparse.ArgumentParser(description='Markdown Template processor')
    parser.add_argument('-f', dest='folder', help='The folder to process (default: all in current directory)')
    return parser.parse_args()


def _main():
    args = _parse_args()
    if args.folder:
        process_dir(args.folder)
    else:
        for dirname in os.listdir('.'):
            process_dir(dirname)


if __name__ == '__main__':
    sys.exit(_main())