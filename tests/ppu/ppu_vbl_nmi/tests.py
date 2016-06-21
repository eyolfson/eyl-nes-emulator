TEST_01_ROM = (
	"01-vbl_basics.nes",
	b"06aea5af4edab4e3141c939cd5ac9936f8758203b25dcaf84ae1a09db49e024a"
)
TEST_01_BIN = (
	"01.bin",
	b"a9ad96191e457688027dec67c336622b88e3a252f30b5a0e29a6b33c8997e89c"
)

TEST_02_ROM = (
	"02-vbl_set_time.nes",
	b"dd98856130078844e3aa4bd95a9be8ab501ea84c089f1d8ad49a1b20af4b3a80"
)
TEST_02_BIN = (
	"02.bin",
	b"367721ee9a9ad697d7196d5de497a6c7d91a7c61e2526d772eb916035ec42842"
)

TEST_03_ROM = (
	"03-vbl_clear_time.nes",
	b"787fdaa4dd6c5b6df5f4308fb6d55b57e2c2f69bd5ecdf8ad5c69735db4fcc72"
)
TEST_03_BIN = (
	"03.bin",
	b"07640328238be855577873465e2d7e445658d078064ccdc15aef8c5208155f3f"
)

TEST_04_ROM = (
	"04-nmi_control.nes",
	b"84722c75b896c47c8642f83220230fe14f0a31e55e26ecb83c400e6a26d91b32"
)
TEST_04_BIN = ("04.bin",
	b"98015df0197e34f354fafca9d14eb8763c842761a38a88b5709ebc5335db0863"
)

TEST_05_ROM = (
	"05-nmi_timing.nes",
	b"72e515d689d7404ae5779b8c9c4c7b3563a755a94bd44864516f1b03df044482"
)

TEST_06_ROM = (
	"06-suppression.nes",
	b"811dd5997bbf48c2e5687ab06845f17ea76b2be472786596c334137582cc72aa"
)

TEST_07_ROM = (
	"07-nmi_on_timing.nes",
	b"1ed154363660b5775b112ae63ce9bb4e400ebde2afef4d0ac12fc433efda3702"
)

TEST_08_ROM = (
	"08-nmi_off_timing.nes",
	b"1d2a4093091c8e58a7f99d6a3531bbc6346b52cfc59bcb17ca04c1f2376cf2fc"
)

TEST_09_ROM = (
	"09-even_odd_frames.nes",
	b"1ac04283021ddd9294cc74ee709c55e20a350dc4815c15a8a93b3654837e858d"
)

TEST_10_ROM = (
	"10-even_odd_timing.nes",
	b"7217d2d172ce11ad45c4da40c2f22201cf0eb758bc2cd8dd39d2cf0a7d4ca83e"
)

DATA = [
	(TEST_01_ROM, TEST_01_BIN, '141'),
	(TEST_02_ROM, TEST_02_BIN, '170'),
	(TEST_03_ROM, TEST_03_BIN, '170'),
	(TEST_04_ROM, TEST_04_BIN, '33'),
	(TEST_05_ROM, None, None),
	(TEST_06_ROM, None, None),
	(TEST_07_ROM, None, None),
	(TEST_08_ROM, None, None),
	(TEST_09_ROM, None, None),
	(TEST_10_ROM, None, None),
]
