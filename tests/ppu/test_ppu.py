import importlib
import os
import subprocess

EXECUTABLE = "build/nes-emulator-ppu"

def check_build():
	os.makedirs("build", exist_ok=True)
	try:
		subprocess.run(["cmake", "../src"], cwd="build", check=True)
		subprocess.run(["make"], cwd="build", check=True)
	except subprocess.CalledProcessError:
		return False
	return True

def check_file(filename, sha256):
	completed_process = subprocess.run(["sha256sum", filename],
	                                   stdout=subprocess.PIPE)
	return completed_process.stdout.startswith(sha256)

def run_tests(suite):
	print("")
	print("# {}".format(suite))
	module = importlib.import_module("{}.tests".format(suite))
	data = module.DATA
	tests_passed = 0
	for rom, expected_bin, expected_frame in data:
		rom_filename_base = rom[0]
		rom_filename = "{}/{}".format(suite, rom_filename_base)
		rom_sha256 = rom[1]
		print(rom_filename_base[:-4], ": ", sep="", end="")
		if not check_file(rom_filename, rom_sha256):
			print("Check FAILED")
			continue

		if expected_bin == None:
			print("Expected MISSING")
			continue
		bin_filename_base = expected_bin[0]
		bin_filename = "{}/{}".format(suite, bin_filename_base)
		bin_sha256 = expected_bin[1]
		if not check_file(bin_filename, bin_sha256):
			print("Check FAILED")
			continue
		
		completed_process = subprocess.run([EXECUTABLE,
		                                    rom_filename,
		                                    bin_filename,
		                                    expected_frame],
		                                   stdout=subprocess.PIPE)
		if completed_process.returncode != 0:
			print("Process FAILED")
			continue
		lines = completed_process.stdout.splitlines()
		last_line = lines[-1].decode()
		if last_line == "Test SUCCESS":
			tests_passed += 1
		print(last_line)
	results = (tests_passed, len(data))
	print("{}/{} tests passed".format(results[0], results[1]))
	return results

def run_all():
	all_results = (0, 0)
	for suite in ["blargg_ppu_tests", "ppu_vbl_nmi", "sprite_hit_tests"]:
		results = run_tests(suite)
		all_results = (all_results[0] + results[0],
		               all_results[1] + results[1])
	return all_results


if __name__ == "__main__":
	if check_build():
		results = run_all()
		print()
		print("{}/{} tests passed".format(results[0], results[1]))
