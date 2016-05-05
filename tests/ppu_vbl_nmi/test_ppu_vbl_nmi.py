import os
import subprocess

REQUIRED_FILES = [
	("01-vbl_basics.nes",
	 b"06aea5af4edab4e3141c939cd5ac9936f8758203b25dcaf84ae1a09db49e024a"),
	("02-vbl_set_time.nes",
	 b"dd98856130078844e3aa4bd95a9be8ab501ea84c089f1d8ad49a1b20af4b3a80"),
	("03-vbl_clear_time.nes",
	 b"787fdaa4dd6c5b6df5f4308fb6d55b57e2c2f69bd5ecdf8ad5c69735db4fcc72"),
	("04-nmi_control.nes",
	 b"84722c75b896c47c8642f83220230fe14f0a31e55e26ecb83c400e6a26d91b32"),
	("05-nmi_timing.nes",
	 b"72e515d689d7404ae5779b8c9c4c7b3563a755a94bd44864516f1b03df044482"),
	("06-suppression.nes",
	 b"811dd5997bbf48c2e5687ab06845f17ea76b2be472786596c334137582cc72aa"),
	("07-nmi_on_timing.nes",
	 b"1ed154363660b5775b112ae63ce9bb4e400ebde2afef4d0ac12fc433efda3702"),
	("08-nmi_off_timing.nes",
	 b"1d2a4093091c8e58a7f99d6a3531bbc6346b52cfc59bcb17ca04c1f2376cf2fc"),
	("09-even_odd_frames.nes",
	 b"1ac04283021ddd9294cc74ee709c55e20a350dc4815c15a8a93b3654837e858d"),
	("10-even_odd_timing.nes",
	 b"7217d2d172ce11ad45c4da40c2f22201cf0eb758bc2cd8dd39d2cf0a7d4ca83e"),
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

EXPECTED_TESTS_PASSED = 10

def run_tests():
	tests_passed = 0
	for i in range(EXPECTED_TESTS_PASSED):
		rom_file = REQUIRED_FILES[i][0]
		test_prefix = rom_file[:2]
		executable = "build/nes-emulator-ppu_vbl_nmi-{}".format(
			test_prefix)
		completed_process = subprocess.run([executable,
		                                    rom_file],
		                                   stdout=subprocess.PIPE)
		lines = completed_process.stdout.splitlines()
		last_line = lines[-1].decode()
		if last_line == "Check SUCCESS":
			tests_passed += 1
		print("{}: {}".format(test_prefix, last_line))
		break
	return tests_passed

if __name__ == "__main__":
	if check_files() and check_build():
		tests_passed = run_tests()
		print()
		print("{}/{} tests passed".format(tests_passed,
		                                  EXPECTED_TESTS_PASSED))
