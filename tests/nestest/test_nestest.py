import os
import subprocess

REQUIRED_FILES = [
	("nestest.nes",
	 b"f67d55fd6b3cf0bad1cc85f1df0d739c65b53e79cecb7fea8f77ec0eadab0004"),
	("nestest.log",
	 b"bd9fa3a1f4f298aed19762368037df7ebb1928250528b708d940f2e1108a355c"),
]

def check_files():
	checks_passed = True
	for filename, sha256sum in REQUIRED_FILES:
		completed_process = subprocess.run(["sha256sum", filename],
		                                   stdout=subprocess.PIPE)
		print(filename, end=' ')
		if completed_process.stdout.startswith(sha256sum):
			print("PASS")
		else:
			print("FAIL")
			checks_passed = False
	return checks_passed

def check_build():
	os.makedirs("build", exist_ok=True)
	try:
		subprocess.run(["cmake", "../src"], cwd="build", check=True)
		subprocess.run(["make"], cwd="build", check=True)
	except subprocess.CalledProcessError:
		return False
	return True

def check_line(expected_line, actual_line):
	CHECKS = [
		("PC", 0, 4),
		("A", 50, 52),
		("X", 55, 57),
		("Y", 60, 62),
		("P", 65, 67),
		("S", 71, 73),
	]
	checks_passed = True
	for check in CHECKS:
		expected = expected_line[check[1]:check[2]]
		actual = actual_line[check[1]:check[2]]
		if expected != actual:
			print()
			print("{} mismatched, expected: {}, actual: {}".format(
				check[0], expected.decode(), actual.decode()))
			checks_passed = False
	return checks_passed

EXPECTED_LINES_PASSED = 8991

def run_test():
	lines_passed = 0
	completed_process = subprocess.run(["build/nes-emulator-nestest",
	                                    "nestest.nes"],
	                                   stdout=subprocess.PIPE)
	lines = completed_process.stdout.splitlines()
	with open("nestest.log", "rb") as f:
		for line in f:
			if len(lines) == lines_passed:
				print()
				print("missing input after:")
				print(lines[lines_passed - 1].decode())
				return lines_passed
			if not check_line(line, lines[lines_passed]):
				return lines_passed
			lines_passed += 1
	return lines_passed

if __name__ == "__main__":
	if check_files() and check_build():
		lines_passed = run_test()
		print()
		print("{}/{} lines passed".format(lines_passed,
		                                  EXPECTED_LINES_PASSED))
