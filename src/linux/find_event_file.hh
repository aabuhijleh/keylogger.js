#pragma once

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * Tries to find the filepath of a connected keyboard
 * \return malloc allocated filepath of keyboard, or NULL if none could be
 *         detected
 */
char *get_keyboard_event_file(void);