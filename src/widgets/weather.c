#include "widgets.h"
#include "weather.h"

static int
get_geoip_location (struct location *location) {
	json_t *location_data, *geoip_city, *geoip_country_code;
	json_error_t error;

	char *geoip_raw_json = candybar_curl_request("http://freegeoip.net/json/");
	location_data = json_loads(geoip_raw_json, 0, &error);

	if (!location_data) {
		LOG_WARN("error while fetching GeoIP data");
		goto error;
	}

	free(geoip_raw_json);
	geoip_raw_json = NULL;

	geoip_city = json_object_get(location_data, "city");
	if (!json_is_string(geoip_city)) {
		LOG_ERR("received GeoIP city is not a string");
		goto error;
	}
	geoip_country_code = json_object_get(location_data, "country_code");
	if (!json_is_string(geoip_country_code)) {
		LOG_ERR("received GeoIP country code is not a string");
		goto error;
	}

	location->city = strdup(json_string_value(geoip_city));
	location->country_code = strdup(json_string_value(geoip_country_code));

	if (location_data != NULL) {
		json_decref(location_data);
	}

	return 0;

 error:
	free(geoip_raw_json);
	geoip_raw_json = NULL;

	json_decref(location_data);

	return 1;
}

static struct weather*
get_weather_information (struct location *location) {
	int query_str_len, request_uri_len;
	char *query_str, *query_str_escaped, *request_uri;
	char *query_str_template = "select item.condition.code, item.condition.temp from weather.forecast "
	                           "where u = 'c' and woeid in (select woeid from geo.places where text = '%s %s' limit 1) limit 1;";
	char *request_uri_template = "http://query.yahooapis.com/v1/public/yql?q=%s&format=json";
	json_t *weather_data;
	json_error_t error;
	struct weather *weather = calloc(1, sizeof(struct weather));
	CURL *curl;

	curl = curl_easy_init();

	query_str_len = snprintf(NULL, 0, query_str_template, location->city, location->country_code);
	query_str = malloc(query_str_len + 1);
	snprintf(query_str, query_str_len + 1, query_str_template, location->city, location->country_code);

	query_str_escaped = curl_easy_escape(curl, query_str, 0);
	request_uri_len = snprintf(NULL, 0, request_uri_template, query_str_escaped);
	request_uri = malloc(request_uri_len + 1);
	snprintf(request_uri, request_uri_len + 1, request_uri_template, query_str_escaped);

	char *weather_raw_json = candybar_curl_request(request_uri);
	weather_data = json_loads(weather_raw_json, 0, &error);

	free(query_str);
	query_str = NULL;
	free(query_str_escaped);
	query_str_escaped = NULL;
	free(request_uri);
	request_uri = NULL;
	curl_easy_cleanup(curl);
	curl_global_cleanup();

	if (!weather_data) {
		goto error;
	}

	free(weather_raw_json);
	weather_raw_json = NULL;

	json_t *weather_code, *weather_temp;
	json_t *tmp_obj = NULL;
	tmp_obj = json_object_get(weather_data, "query");
	tmp_obj = json_object_get(tmp_obj, "results");
	tmp_obj = json_object_get(tmp_obj, "channel");
	tmp_obj = json_object_get(tmp_obj, "item");
	tmp_obj = json_object_get(tmp_obj, "condition");
	if (!json_is_object(tmp_obj)) {
		LOG_ERR("invalid weather data object received");
		goto error;
	}

	weather_code = json_object_get(tmp_obj, "code");
	weather_temp = json_object_get(tmp_obj, "temp");

	if (!json_is_string(weather_code) || !json_is_string(weather_temp)) {
		LOG_ERR("invalid weather query result received (weather code or temp missing)");
		if (tmp_obj != NULL) {
			json_decref(tmp_obj);
		}
		goto error;
	}

	int int_val;
	char *end;

	int_val = strtol(json_string_value(weather_code), &end, 10);
	if (*end) {
		LOG_WARN("received weather code is not an integer");
	}
	else {
		weather->code = int_val;
	}

	int_val = strtol(json_string_value(weather_temp), &end, 10);
	if (*end) {
		LOG_WARN("received temperature is not an integer");
	}
	else {
		weather->temp = int_val;
	}

	if (tmp_obj != NULL) {
		json_decref(tmp_obj);
	}
	if (weather_data != NULL) {
		json_decref(weather_data);
	}

	return weather;

 error:
	json_decref(weather_data);

	free(weather);
	weather = NULL;

	return NULL;
}

static int
widget_update (struct widget *widget, struct location *location, struct widget_config config) {
	struct weather *weather;

	weather = get_weather_information(location);
	if (!weather) {
		LOG_ERR("error while fetching weather data");

		return -1;
	}

	widget_data_callback(widget,
	                     widget_data_arg_number(weather->code),
	                     widget_data_arg_number(weather->temp),
	                     widget_data_arg_string(config.unit));

	free(weather);
	weather = NULL;

	return 0;
}

void*
widget_main (struct widget *widget) {
	struct widget_config config = widget_config_defaults;
	widget_init_config_string(widget->config, "location", config.location);
	widget_init_config_string(widget->config, "unit", config.unit);
	widget_init_config_integer(widget->config, "refresh_interval", config.refresh_interval);
	widget_epoll_init(widget);

	struct location *location = calloc(1, sizeof(location));

	if (strlen(config.location) > 0) {
		location->city = strdup(config.location);
		location->country_code = strdup("");
	}
	else {
		get_geoip_location(location);
	}
	if (!(location->city || location->country_code)) {
		LOG_WARN("could not get GeoIP location, consider setting the location manually in config.json");
		goto cleanup;
	}

	while (true) {
		widget_update(widget, location, config);
		widget_epoll_wait_goto(widget, config.refresh_interval, cleanup);
	}

cleanup:
	free(location->city);
	location->city = NULL;

	free(location->country_code);
	location->country_code = NULL;

	free(location);
	location = NULL;

	widget_epoll_cleanup(widget);
	widget_clean_exit(widget);
}
