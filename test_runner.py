import subprocess
import sys

def test_file(interpreter, script_to_test, expect, err):
    print(f"Running test: {script_to_test}")
    file = open(script_to_test,'r')
    contents = file.readlines()

    expected = [x.replace(expect, "").strip() for x in contents if x.startswith(expect)]
    if len(expected) == 0:
        print("No expected result provided. Test aborted")
        return 1

    result = subprocess.run([interpreter, script_to_test], capture_output=True)
    actual = ""
    if err:
        actual = result.stderr.decode().split('\n')
    else:
        actual = result.stdout.decode().split('\n')
    actual = [x.strip() for x in actual if x != '']

    if len(actual) != len(expected):
        print(f"FAIL: Expect: {expected}, Actual: {actual}")
        return 1

    i = 0
    for line in actual:
        if expected[i] != line:
            print(f"FAIL: Expect '{expected[i]}', Actual '{line}'")
            return 1
        i += 1

    print("PASS")
    return 0

# ENTRY POINT #
args = sys.argv

if len(args) < 4:
    print("Usage: test_runner.py [interpeter] [scripts to test] [expected result prefix] [Flags: -e = expect stderr]")
    exit_code = 64
else:
    interpreter = args[1]
    script_to_test = args[2]
    expect = args[3]
    err = len(args) > 4 and args[4] == "-e"

    exit_code = test_file(interpreter, script_to_test, expect, err)

sys.exit(exit_code)