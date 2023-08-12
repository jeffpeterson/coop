
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "nvs_flash.h"

#include "coop.h"

static const char *TAG = "COOPER";

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
  ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

void attr_cb(uint8_t status, uint8_t endpoint, uint16_t cluster_id,
             uint16_t attr_id, void *new_value) {

  if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
    uint8_t value = *(uint8_t *)new_value;

    if (attr_id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
      /* implemented light on/off control */
      ESP_LOGI(TAG, "on/off light set to %hd", value);
      light_driver_set_power((bool)value);
    }
  } else {
    /* Implement some actions if needed when other cluster changed */
    ESP_LOGI(TAG, "cluster:0x%x, attribute:0x%x changed ", cluster_id, attr_id);
  }
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
  uint32_t *p_sg_p = signal_struct->p_app_signal;
  esp_err_t err_status = signal_struct->esp_err_status;
  esp_zb_app_signal_type_t sig_type = *p_sg_p;

  switch (sig_type) {
  case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
    ESP_LOGI(TAG, "Zigbee stack initialized");
    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
    break;

  case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
  case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
    if (err_status == ESP_OK) {
      ESP_LOGI(TAG, "Start network steering");
      esp_zb_bdb_start_top_level_commissioning(
          ESP_ZB_BDB_MODE_NETWORK_STEERING);
    } else {
      /* commissioning failed */
      ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %d)",
               err_status);
    }
    break;

  case ESP_ZB_BDB_SIGNAL_STEERING:
    if (err_status == ESP_OK) {
      esp_zb_ieee_addr_t extended_pan_id;
      esp_zb_get_extended_pan_id(extended_pan_id);
      ESP_LOGI(TAG,
               "Joined network successfully (Extended PAN ID: "
               "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, "
               "Channel:%d)",
               extended_pan_id[7], extended_pan_id[6], extended_pan_id[5],
               extended_pan_id[4], extended_pan_id[3], extended_pan_id[2],
               extended_pan_id[1], extended_pan_id[0], esp_zb_get_pan_id(),
               esp_zb_get_current_channel());
    } else {
      ESP_LOGI(TAG, "Network steering was not successful (status: %d)",
               err_status);
      esp_zb_scheduler_alarm(
          (esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
          ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
    }
    break;
  default:
    ESP_LOGI(TAG, "ZDO signal: %d, status: %d", sig_type, err_status);
    break;
  }
}

static esp_zb_cfg_t network_config = {
    .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,
    .install_code_policy = false, // Enable for security
    .nwk_cfg.zed_cfg =
        {
            .ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN,
            .keep_alive = 3000, // Milliseconds
        },
};

// Are these required to be different?
static uint8_t light_endpoint_id = 10;
static uint8_t shade_endpoint_id = 11;

static const char *model = "Cooper";
static const char *manufacturer = "Craft & Concept";

static void esp_zb_task(void *pvParameters) {
  /* initialize Zigbee stack with Zigbee end-device config */

  esp_zb_init(&network_config);

  esp_zb_attribute_list_t *basic_cluster =
      esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
  esp_zb_basic_cluster_add_attr(
      basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, &model);
  esp_zb_basic_cluster_add_attr(
      basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, &manufacturer);

  /* set the on-off light device config */
  esp_zb_on_off_light_cfg_t light_config = ESP_ZB_DEFAULT_ON_OFF_LIGHT_CONFIG();
  esp_zb_ep_list_t *light_endpoint =
      esp_zb_on_off_light_ep_create(light_endpoint_id, &light_config);

  // Set the shade device config
  esp_zb_shade_cfg_t shade_config = ESP_ZB_DEFAULT_SHADE_CONFIG();
  esp_zb_ep_list_t *shade_endpoint =
      esp_zb_shade_ep_create(shade_endpoint_id, &shade_config);

  esp_zb_device_register(light_endpoint);
  esp_zb_device_register(shade_endpoint);

  esp_zb_device_add_set_attr_value_cb(attr_cb);

  ESP_ERROR_CHECK(esp_zb_start(false));
  esp_zb_main_loop_iteration();
}

void app_main(void) {
  esp_zb_platform_config_t config = {
      .radio_config = {.radio_mode = RADIO_MODE_NATIVE},
      .host_config = {.host_connection_mode = HOST_CONNECTION_MODE_NONE},
  };

  ESP_ERROR_CHECK(nvs_flash_init());
  /* load Zigbee light_bulb platform config to initialization */
  ESP_ERROR_CHECK(esp_zb_platform_config(&config));

  /* hardware related and device init */
  light_driver_init(LIGHT_DEFAULT_OFF);
  xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
