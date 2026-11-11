"""Derive firmware/Source/app.zpscfg from the NXP JN-AN-1218 DimmableLight node.

Keeps the Coordinator node (referenced for the TC link key) and turns the
DimmableLight router into "AqaraRelay" with the endpoints of the factory
LLKZMK11LM firmware.

    python make_zpscfg.py <JN-AN-1218>/Common_Light/Source/app_gp_notl.zpscfg firmware/Source/app.zpscfg
"""
import re
import sys

src, dst = sys.argv[1], sys.argv[2]
text = open(src, encoding="utf-8").read()

# split into header (up to first ChildNodes), the child nodes and the tail
first = text.index("<ChildNodes ")
first = text.rindex("\n", 0, first) + 1
head, rest = text[:first], text[first:]
nodes = re.findall(r"  <ChildNodes .*?</ChildNodes>\n", rest, re.S)
node = next(n for n in nodes if 'Name="DimmableLight"' in n)
tail = rest[rest.rindex("</ChildNodes>") + len("</ChildNodes>\n"):]

node = node.replace("DimmableLight->", "AqaraRelay->")
node = node.replace('Name="DimmableLight"', 'Name="AqaraRelay"')
node = node.replace('InterPAN="true"', 'InterPAN="false"')
node = node.replace('GreenPowerSupport="true"', 'GreenPowerSupport="false"')
# must be >= CLD_GROUPS_MAX_NUMBER_OF_GROUPS in zcl_options.h or endpoint registration fails
node = re.sub(r'<GroupTable Size="\d+"/>', '<GroupTable Size="8"/>', node)
node = re.sub(r'\s*<GreenPowerTxQueue[^>]*/>', "", node)
node = re.sub(r'\s*<GreenPowerSecurityTable[^>]*/>', "", node)
node = re.sub(r'(<ChannelMask )[^>]*(/>)',
              lambda m: m.group(1) + " ".join(f'Channel{c}="true"' for c in range(11, 27)) + m.group(2),
              node)

# Simple descriptors of the factory firmware, as interviewed by Zigbee2MQTT:
# both endpoints are device 257 (0x0101) in the HA profile.
EP_CLUSTERS = {
    1: (["Basic", "Identify", "Groups", "Scenes", "PowerConfiguration", "DeviceTemperature",
         "Time", "OnOff", "BinaryOutput", "ElectricalMeasurement", "AnalogInput"],
        ["OTA", "Time"]),
    2: (["OnOff", "BinaryOutput", "Groups", "Scenes"], []),
}

# clusters the ZLO profile of the example does not list
EXTRA_PROFILE_CLUSTERS = [
    ("PowerConfiguration", 0x0001), ("DeviceTemperature", 0x0002), ("Time", 0x000A),
    ("AnalogInput", 0x000C), ("BinaryOutput", 0x0010), ("OTA", 0x0019),
    ("ElectricalMeasurement", 0x0B04),
]
NL = "\n"


def endpoint(ep, name):
    inputs, outputs = EP_CLUSTERS[ep]
    apdu = "AqaraRelay->apduZCL"
    lines = [f'    <Endpoints Id="{ep}" Enabled="true" ApplicationDeviceId="257" '
             f'ApplicationDeviceVersion="1" Profile="ZLO" Message="APP_ZCL_vEventHandler" Name="{name}">']
    for c in inputs:
        lines.append(f'      <InputClusters Cluster="{c}" RxAPDU="{apdu}" Discoverable="true"/>')
    lines.append(f'      <InputClusters Cluster="Default" RxAPDU="{apdu}" Discoverable="false"/>')
    for c in outputs:
        lines.append(f'      <OutputClusters Cluster="{c}" TxAPDUs="{apdu}" Discoverable="true"/>')
    lines.append("    </Endpoints>")
    return NL.join(lines) + NL


def add_profile_clusters(text):
    start = text.index('<Profiles Name="ZLO"')
    end = text.index("  </Profiles>", start)
    block = text[start:end]
    extra = "".join(f'    <Clusters Name="{n}" Id="0x{i:04X}"/>{NL}' for n, i in EXTRA_PROFILE_CLUSTERS
                    if f'Name="{n}"' not in block)
    return text[:end] + extra + text[end:]


# replace the LIGHT and GreenPower endpoints with the two relay endpoints
node = re.sub(r'    <Endpoints Id="1" .*?</Endpoints>\n', lambda m: endpoint(1, "RELAY1") + endpoint(2, "RELAY2"), node, flags=re.S)
node = re.sub(r'    <Endpoints Id="242" .*?</Endpoints>\n', "", node, flags=re.S)

assert node.count("<Endpoints ") == 3, node.count("<Endpoints ")
open(dst, "w", encoding="utf-8", newline=NL).write(add_profile_clusters(head) + node + tail)
print("wrote", dst)
