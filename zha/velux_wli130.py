"""ZHA quirk for the VELUX WLI 130 IR bridge (FireBeetle 2 ESP32-C6 firmware).

Zigbee has no per-endpoint names, so without this ZHA calls the eight
window coverings "Cover", "Cover 2", ... This quirk names each one after
kPanels[] in Config.h (motor[] and "all").

Install: copy this file into Home Assistant's custom quirks folder, e.g.
/config/custom_zha_quirks/, and point ZHA at it in configuration.yaml:

    zha:
      custom_quirks_path: /config/custom_zha_quirks/

then restart Home Assistant and remove + re-pair the device (or use
"Reconfigure" on the device page) so ZHA applies the quirk. Needs a recent
ZHA (quirks v2 with change_entity_metadata). If the names don't change,
rename the entities by hand in HA - the endpoint numbers below tell which
is which.

Types: the firmware announces windows as roller shades and blinds as exterior
roller shades (see kPanels[].kind in Config.h). For a window icon: entity
settings -> "Show as" -> Window.

Keep the table below in sync with kPanels[], kZigbeeFirstEndpoint,
kZigbeeManufacturer and kZigbeeModel in Config.h: endpoint =
kZigbeeFirstEndpoint + keypad * 4 + motor, motor 3 = all three.
"""

from zigpy.quirks.v2 import QuirkBuilder
from zigpy.zcl.clusters.closures import WindowCovering
from zigpy.zcl.clusters.general import OnOff

MANUFACTURER = "VELUX"                # kZigbeeManufacturer
MODEL = "WLI 130 IR"                  # kZigbeeModel

COVERS = {
    10: "Window 1",
    11: "Window 2",
    12: "Window 3",
    13: "All windows",
    14: "Blind 1",
    15: "Blind 2",
    16: "Blind 3",
    17: "All blinds",
}

USB_LOG_ENDPOINT = 30                 # kZigbeeUsbLogEndpoint: On/Off switch

builder = QuirkBuilder(MANUFACTURER, MODEL)
for endpoint_id, name in COVERS.items():
    builder = builder.change_entity_metadata(
        endpoint_id=endpoint_id,
        cluster_id=WindowCovering.cluster_id,
        new_fallback_name=name,
    )
builder = builder.change_entity_metadata(
    endpoint_id=USB_LOG_ENDPOINT,
    cluster_id=OnOff.cluster_id,
    new_fallback_name="USB log",
)
builder.add_to_registry()
