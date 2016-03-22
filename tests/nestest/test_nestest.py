import subprocess

REQUIRED_FILES = [
	("nestest.nes",
	 b"f67d55fd6b3cf0bad1cc85f1df0d739c65b53e79cecb7fea8f77ec0eadab0004"),
	("nestest.log",
	 b"bd9fa3a1f4f298aed19762368037df7ebb1928250528b708d940f2e1108a355c"),
]

def check_files():
	for filename, sha256sum in REQUIRED_FILES:
		completed_process = subprocess.run(["sha256sum", filename],
		                                   stdout=subprocess.PIPE)
		print(filename, end=' ')
		if completed_process.stdout.startswith(sha256sum):
			print("PASS")
		else:
			print("FAIL")

if __name__ == "__main__":
	check_files()
