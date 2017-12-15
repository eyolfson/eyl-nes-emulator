#include "evdev.h"

#include "../exit_code.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>

#include <libevdev/libevdev.h>

struct evdev_backend {
	int fd;
	struct libevdev *dev;

	uint8_t controller1_state;
	uint8_t controller1_press;
};

static void handle_key(struct evdev_backend *e, uint16_t code, int32_t value)
{
	const uint8_t BUTTON_A      = 1 << 7;
	const uint8_t BUTTON_B      = 1 << 6;
	const uint8_t BUTTON_SELECT = 1 << 5;
	const uint8_t BUTTON_START  = 1 << 4;

	switch (value) {
	case 0:
		switch (code) {
		case BTN_SELECT:
			e->controller1_press &= ~(BUTTON_SELECT);
			break;
		case BTN_START:
			e->controller1_press &= ~(BUTTON_START);
			break;
		case BTN_WEST:
			e->controller1_press &= ~(BUTTON_B);
			break;
		case BTN_SOUTH:
			e->controller1_press &= ~(BUTTON_A);
			break;
		}
		break;
	case 1:
		switch (code) {
		case BTN_SELECT:
			e->controller1_press |= BUTTON_SELECT;
			e->controller1_state |= BUTTON_SELECT;
			break;
		case BTN_START:
			e->controller1_press |= BUTTON_START;
			e->controller1_state |= BUTTON_START;
			break;
		case BTN_WEST:
			e->controller1_press |= BUTTON_B;
			e->controller1_state |= BUTTON_B;
			break;
		case BTN_SOUTH:
			e->controller1_press |= BUTTON_A;
			e->controller1_state |= BUTTON_A;
			break;
		}
		break;
	}
}

static void handle_abs_y(struct evdev_backend *e, int32_t value)
{
	const uint8_t BUTTON_UP     = 1 << 3;
	const uint8_t BUTTON_DOWN   = 1 << 2;

	switch (value) {
	case -1:
		e->controller1_press |= BUTTON_UP;
		e->controller1_state |= BUTTON_UP;
		break;
	case 1:
		e->controller1_press |= BUTTON_DOWN;
		e->controller1_state |= BUTTON_DOWN;
		break;
	case 0:
		e->controller1_press &= ~(BUTTON_UP);
		e->controller1_press &= ~(BUTTON_DOWN);
		break;
	}
}

static void handle_abs_x(struct evdev_backend *e, int32_t value)
{
	const uint8_t BUTTON_LEFT   = 1 << 1;
	const uint8_t BUTTON_RIGHT  = 1 << 0;

	switch (value) {
	case -1:
		e->controller1_press |= BUTTON_LEFT;
		e->controller1_state |= BUTTON_LEFT;
		break;
	case 1:
		e->controller1_press |= BUTTON_RIGHT;
		e->controller1_state |= BUTTON_RIGHT;
		break;
	case 0:
		e->controller1_press &= ~(BUTTON_LEFT);
		e->controller1_press &= ~(BUTTON_RIGHT);
		break;
	}
}

static uint8_t controller1_read(void *pointer)
{
	struct evdev_backend *e = pointer;

	int rc;
	do {
		struct input_event ev;
		rc = libevdev_next_event(e->dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
		if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
			if (ev.type == EV_KEY) {
				handle_key(e, ev.code, ev.value);
			}
			else if (ev.type == EV_ABS && ev.code == ABS_HAT0Y) {
				handle_abs_y(e, ev.value);
			}
			else if (ev.type == EV_ABS && ev.code == ABS_HAT0X) {
				handle_abs_x(e, ev.value);
			}
		}
	} while (rc != -EAGAIN);

	uint8_t state = e->controller1_state;
	e->controller1_state = e->controller1_press;

	return state;
}

uint8_t nes_emulator_backend_evdev_init(
	struct nes_emulator_controller_backend **controller_backend)
{
	if (controller_backend == NULL) {
		return EXIT_CODE_ARG_ERROR_BIT;
	}
	*controller_backend = NULL;

	struct nes_emulator_controller_backend *b =
		malloc(sizeof(struct nes_emulator_controller_backend));
	if (b == NULL) {
		return EXIT_CODE_OS_ERROR_BIT;
	}

	struct evdev_backend *e = malloc(sizeof(struct evdev_backend));
	if (e == NULL) {
		free(e);
		return EXIT_CODE_OS_ERROR_BIT;
	}
	e->controller1_state = 0;
	e->controller1_press = 0;

	int fd = open("/dev/input/event23", O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		free(e);
		free(b);
		return EXIT_CODE_OS_ERROR_BIT;
	}

	int rc = libevdev_new_from_fd(fd, &e->dev);
	if (rc < 0) {
		close(fd);
		free(e);
		free(b);
		return EXIT_CODE_EVDEV_ERROR_BIT;
	}

	b->pointer = e;
	b->controller1_read = controller1_read;
	*controller_backend = b;
	return 0;
}

uint8_t nes_emulator_backend_evdev_fini(
	struct nes_emulator_controller_backend **controller_backend)
{
	struct evdev_backend *e = (struct evdev_backend *)
	                          (*controller_backend)->pointer;
	libevdev_free(e->dev);
	close(e->fd);
	free(e);
	free(*controller_backend);
	*controller_backend = NULL;
	return 0;
}
