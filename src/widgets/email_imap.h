#include "util/curl.h"

static struct widget_config {
	const char *address;
	const char *username;
	const char *password;
	const char *password_command;
	bool ssl_verify;
	int refresh_interval;
} widget_config_defaults = {
	.address = "imaps://imap.gmail.com:993/INBOX",
	.username = "",
	.password = "",
	.password_command = "",
	.ssl_verify = true,
	.refresh_interval = 60,
};
