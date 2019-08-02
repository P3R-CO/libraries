/*
 * mos.yml:
config_schema:
  - ["mdash.device", "s", "", {}]
  - ["mdash.api_key", "s", "", {}]
 */

static void mdash_api_handler(struct mg_connection *nc, int ev, void *evd,
                              void *cb_arg) {
  switch (ev) {
    case MG_EV_CONNECT: {
      int err = *(int *) evd;
      if (err != 0) {
        LOG(LL_INFO, ("%s - err: %d (%s)", __FUNCTION__, err, strerror(err)));
      }
      break;
    }
    case MG_EV_HTTP_REPLY: {
      const struct http_message *msg = (const struct http_message *) (evd);
      if (msg->resp_code != 200) {
        LOG(LL_INFO, ("%s - http reply error: %d (%.*s)", __FUNCTION__,
                      msg->resp_code, msg->body.len, msg->body.p));
      }
      nc->flags |= MG_F_CLOSE_IMMEDIATELY;
      break;
    }
  }
  (void) cb_arg;
}

static void mdash_connect(const char *device, const char *api_key,
                          const char *post_data) {
  char url[128];
  snprintf(url, sizeof(url),
           "https://mdash.net/api/v2/devices/%s?access_token=%s", device,
           api_key);
  const char *extra_headers = "Content-Type: application/json\r\n";
  struct mg_mgr *mgr = mgos_get_mgr();
  struct mg_connection *conn = mg_connect_http(mgr, mdash_api_handler, NULL,
                                               url, extra_headers, post_data);
  LOG(LL_INFO, ("%s - conn: %p", __FUNCTION__, conn));
}

static bool mdash_get_device_api_key(const char **pdevice,
                                     const char **papi_key) {
  const char *device = mgos_sys_config_get_mdash_device();
  const char *api_key = mgos_sys_config_get_mdash_api_key();
  if ((device == NULL) || (api_key == NULL)) {
    LOG(LL_ERROR, ("%s - device and/or api_key is NULL", __FUNCTION__));
    return false;
  }
  *pdevice = device;
  *papi_key = api_key;
  return true;
}

/*
 * if label is NULL, device.id is used
 */
void mdash_set_label(const char *label) {
  const char *device = NULL;
  const char *api_key = NULL;
  if (!mdash_get_device_api_key(&device, &api_key)) {
    return;
  }

  if (label == NULL) {
    label = mgos_sys_config_get_device_id();
  }

  struct mbuf jsmb;
  struct json_out jsout = JSON_OUT_MBUF(&jsmb);
  mbuf_init(&jsmb, 0);
  json_printf(&jsout, "{shadow:{tags:{labels:%Q}}}", label);
  // ensure null terminating string
  mbuf_append(&jsmb, "", 1);

  mdash_connect(device, api_key, jsmb.buf);
  mbuf_free(&jsmb);
}
