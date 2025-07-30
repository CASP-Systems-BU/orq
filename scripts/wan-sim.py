#!/usr/bin/env python3

import argparse
import subprocess
import sys

# ORQ simulated WAN uses 6Gbps and 20 ms RTT.
BANDWIDTH_LIMIT = "6Gbit"

# this is the PER LINK latency; actual RTT will be double.
# e.g. 40ms => 80ms RTT.
LATENCY = "10ms"

def get_interface_for_host(host):
    try:
        print(f"Checking {host} => ", end='')
        ip = subprocess.getoutput(f"dig +short {host}")
        print(f"{ip} => ", end='')
        result = subprocess.run(['ip', 'route', 'get', ip], capture_output=True, text=True, check=True)
        output = result.stdout.split()
        if "dev" in output:
            iface = output[output.index("dev") + 1]
            print(iface)
            return iface
    except subprocess.CalledProcessError:
        pass

    print("???")
    return None

def main():
    parser = argparse.ArgumentParser(description="Configure WAN simulator")

    parser.add_argument('state', choices=['on', 'off'], help="Turn simulator on or off")

    iface_group = parser.add_mutually_exclusive_group(required=True)
    iface_group.add_argument('-i', '--iface', type=str, help="Interface to simulate WAN over")
    iface_group.add_argument('-H', '--host', type=str, help="Auto-determine interface via this host")

    args = parser.parse_args()

    if args.iface:
        iface = args.iface
    if args.host:
        iface = get_interface_for_host(args.host)
        if not iface:
            print(f"Error: Unable to determine the interface for host {args.host}.", file=sys.stderr)
            sys.exit(1)

    # need to reset either way
    subprocess.run(f"sudo tc qdisc del dev {iface} root", shell=True)

    if args.state == 'on':
        print(f"Enabling WAN on {iface}@ {BANDWIDTH_LIMIT}, {LATENCY}")
        subprocess.run(f'sudo tc qdisc add dev {iface} root netem rate {BANDWIDTH_LIMIT} delay {LATENCY}',
                        shell=True)
    else:
        print(f"Disabled WAN on {iface}...")

if __name__ == "__main__":
    main()
