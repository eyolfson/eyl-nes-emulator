import os
import subprocess

REQUIRED_FILES = [
	("01.basics.nes",
	 b"51819e8e502bd88fe3b7244198a074dbeef2e848f66c587be04b04f1f0d4bb52"),
	("02.alignment.nes",
	 b"125bbb3ce1e67370f1f4559c2ad3221e52a3e98880b9789400292b5f3a8b39e6"),
	("03.corners.nes",
	 b"9dd57776bc6267fe6183c5521d67cbe3fccc6662ae545eb2c419949bf39644d3"),
	("04.flip.nes",
	 b"5f7142bddb51b7577f93fa22f9f668efebbeea00346d7255089e1863acb9d46a"),
	("05.left_clip.nes",
	 b"69b329658c17b953f149c2f0de77eb272089df22c815bd2fd3d6f43206791c13"),
	("06.right_edge.nes",
	 b"8e6653fcb869e06873e29e5e4423122ea72ba0bf38f3ba9e39f471420db759a4"),
	("07.screen_bottom.nes",
	 b"05849956f80267838c5b6556310266b794078a4300841cbb36339fd141905a0b"),
	("08.double_height.nes",
	 b"127fd966b6b32d6d88a53c5f59d7e938827783c9ad056091f119be1c4ab21c71"),
	("09.timing_basics.nes",
	 b"311698c717e50150edd0b5fd0016c41de686463205c20efb5630d6adb90859fd"),
	("10.timing_order.nes",
	 b"0f36bc07bfe51c416e3cc1a5231053572aa6b15aa60e6d2fd0568be49b6dc2e9"),
	("11.edge_timing.nes",
	 b"5a7c121f6e76617be88a0a7035c0e402293be5c685c95b97190a8d70835736ab"),
]

CHECK_FRAME = {
	'01': '1',
	'02': '1',
	'03': '1',
	'04': '1',
	'05': '1',
	'06': '1',
	'07': '1',
	'08': '1',
	'09': '1',
	'10': '1',
	'11': '1',
}

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

EXPECTED_TESTS_PASSED = 11

def run_tests():
	tests_passed = 0
	for i in range(EXPECTED_TESTS_PASSED):
		rom_file = REQUIRED_FILES[i][0]
		test_prefix = rom_file[:2]
		executable = "build/nes-emulator-sprite_hit_tests"
		bin_file = "{}.bin".format(test_prefix)
		completed_process = subprocess.run([executable,
		                                    rom_file,
		                                    bin_file,
		                                    CHECK_FRAME[test_prefix]],
		                                   stdout=subprocess.PIPE)
		if completed_process.returncode != 0:
			print("{}: Process FAILED".format(test_prefix))
			continue
		lines = completed_process.stdout.splitlines()
		last_line = lines[-1].decode()
		if last_line == "Check SUCCESS":
			tests_passed += 1
		print("{}: {}".format(test_prefix, last_line))
	return tests_passed

if __name__ == "__main__":
	if check_files() and check_build():
		tests_passed = run_tests()
		print()
		print("{}/{} tests passed".format(tests_passed,
		                                  EXPECTED_TESTS_PASSED))
