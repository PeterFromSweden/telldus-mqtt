// Parse a tellstick.conf with libconfuse and the option spec of telldus-core
// (service/SettingsConfuse.cpp), and print what it read as JSON.
// Usage: confusedump <tellstick.conf> [print]
// Exit 1 if libconfuse cannot parse the file. With "print", also write the
// file as telldusd writes it (cfg_print) to stderr.
#include <confuse.h>
#include <stdio.h>
#include <string.h>

static void jstr(const char *s) {
	putchar('"');
	for (; s && *s; s++) {
		unsigned char c = (unsigned char)*s;
		if (c == '"' || c == '\\')
			printf("\\%c", c);
		else if (c < 0x20)
			printf("\\u%04x", c);
		else
			putchar(c);
	}
	putchar('"');
}

int main(int argc, char **argv) {
	static const char *params[] = { "devices", "house", "unit", "code", "system", "units", "fade", NULL };
	cfg_opt_t controller_opts[] = {
		CFG_INT("id", -1, CFGF_NONE), CFG_STR("name", "", CFGF_NONE),
		CFG_INT("type", 0, CFGF_NONE), CFG_STR("serial", "", CFGF_NONE), CFG_END()
	};
	cfg_opt_t device_parameter_opts[] = {
		CFG_STR("devices", 0, CFGF_NONE), CFG_STR("house", 0, CFGF_NONE),
		CFG_STR("unit", 0, CFGF_NONE), CFG_STR("code", 0, CFGF_NONE),
		CFG_STR("system", 0, CFGF_NONE), CFG_STR("units", 0, CFGF_NONE),
		CFG_STR("fade", 0, CFGF_NONE), CFG_END()
	};
	cfg_opt_t device_opts[] = {
		CFG_INT("id", -1, CFGF_NONE), CFG_STR("name", "Unnamed", CFGF_NONE),
		CFG_INT("controller", 0, CFGF_NONE), CFG_STR("protocol", "arctech", CFGF_NONE),
		CFG_STR("model", "", CFGF_NONE),
		CFG_SEC("parameters", device_parameter_opts, CFGF_NONE), CFG_END()
	};
	cfg_opt_t opts[] = {
		CFG_STR("user", "nobody", CFGF_NONE), CFG_STR("group", "plugdev", CFGF_NONE),
		CFG_STR("deviceNode", "/dev/tellstick", CFGF_NONE),
		CFG_STR("ignoreControllerConfirmation", "false", CFGF_NONE),
		CFG_SEC("device", device_opts, CFGF_MULTI),
		CFG_SEC("controller", controller_opts, CFGF_MULTI), CFG_END()
	};
	static const char *globals[] = { "user", "group", "deviceNode", "ignoreControllerConfirmation", NULL };
	unsigned int i;
	int j;

	if (argc < 2)
		return 2;
	cfg_t *cfg = cfg_init(opts, CFGF_NOCASE);
	if (cfg_parse(cfg, argv[1]) != CFG_SUCCESS)
		return 1;
	if (argc > 2)
		cfg_print(cfg, stderr);

	putchar('{');
	for (j = 0; globals[j]; j++) {
		jstr(globals[j]);
		putchar(':');
		jstr(cfg_getstr(cfg, globals[j]));
		putchar(',');
	}
	printf("\"devices\":[");
	for (i = 0; i < cfg_size(cfg, "device"); i++) {
		cfg_t *d = cfg_getnsec(cfg, "device", i);
		cfg_t *p = cfg_getsec(d, "parameters");
		printf("%s{\"id\":%ld,\"name\":", i ? "," : "", cfg_getint(d, "id"));
		jstr(cfg_getstr(d, "name"));
		printf(",\"controller\":%ld,\"protocol\":", cfg_getint(d, "controller"));
		jstr(cfg_getstr(d, "protocol"));
		printf(",\"model\":");
		jstr(cfg_getstr(d, "model"));
		printf(",\"parameters\":{");
		const char *sep = "";
		for (j = 0; p && params[j]; j++) {
			if (!cfg_getstr(p, params[j]))
				continue;
			printf("%s", sep);
			jstr(params[j]);
			putchar(':');
			jstr(cfg_getstr(p, params[j]));
			sep = ",";
		}
		printf("}}");
	}
	printf("],\"controllers\":[");
	for (i = 0; i < cfg_size(cfg, "controller"); i++) {
		cfg_t *c = cfg_getnsec(cfg, "controller", i);
		printf("%s{\"id\":%ld,\"name\":", i ? "," : "", cfg_getint(c, "id"));
		jstr(cfg_getstr(c, "name"));
		printf(",\"type\":%ld,\"serial\":", cfg_getint(c, "type"));
		jstr(cfg_getstr(c, "serial"));
		putchar('}');
	}
	printf("]}\n");
	cfg_free(cfg);
	return 0;
}
