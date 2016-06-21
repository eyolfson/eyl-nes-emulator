PALETTE_RAM_ROM = (
	"palette_ram.nes",
	b"11c1de16c67795b247169819a34b9d4dd46165478d6de19d7cd6e7311e906a80"
)

POWER_UP_PALETTE_ROM = (
	"power_up_palette.nes",
	b"4a02958db36d1e1affb6e6b80d556e4a6840847cc42027d2c875fbe917db9f49"
)

SPRITE_RAM_ROM = (
	"sprite_ram.nes",
	b"81275795ea64dfe22f05e35ef3316e6cf84eba40fc9538fc637a08bf263a3110"
)

VBL_CLEAR_TIME_ROM = (
	"vbl_clear_time.nes",
	b"1a1d3ae8062ce0d4f1f3eafa35bf3c3e5df94fe198e7630a09678a60b03b73a0"
)

VRAM_ACCESS_ROM = (
	"vram_access.nes",
	b"a0cde2a3c98d9f62afcba62d4e8f4f4228574ff96ce96fbfdc777e1393c16ee6"
)

COMMON_BIN = (
	"common.bin",
	b"783e416b091a3ec9f138b4cfcb3a6bd5f16175a1f3fcab72673cb0c3f2b509eb"
)

DATA = [
	(PALETTE_RAM_ROM, COMMON_BIN, '19'),
	(POWER_UP_PALETTE_ROM, None, None),
	(SPRITE_RAM_ROM, COMMON_BIN, '18'),
	(VBL_CLEAR_TIME_ROM, None, None),
	(VRAM_ACCESS_ROM, None, None),
]
