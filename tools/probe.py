"""Program this test DK and capture bounded RTT logs using the installed J-Link DLL."""
import argparse
import pathlib
import time
import re
import pylink

p = argparse.ArgumentParser()
p.add_argument('--serial', type=int, default=1057787409)
p.add_argument('--hex', type=pathlib.Path)
p.add_argument('--log', required=True, type=pathlib.Path)
p.add_argument('--seconds', type=float, default=20)
p.add_argument('--rtt-refresh', type=float, default=0,
               help='Reattach RTT periodically to follow target resets (seconds)')
p.add_argument('--rtt-address', type=lambda s: int(s, 0))
p.add_argument('--map', type=pathlib.Path, help='Linker map used to find the exact RTT block')
p.add_argument('--backup', type=pathlib.Path)
p.add_argument('--command', action='append', default=[], help='Send a console command after startup')
args = p.parse_args()
map_path = args.map or (args.hex.with_suffix('.map') if args.hex else None)
if args.rtt_address is None and map_path:
    match = re.search(r'(0x[0-9a-fA-F]+)\s+_SEGGER_RTT\s*$', map_path.read_text(), re.MULTILINE)
    if not match:
        raise ValueError(f'RTT control block not found in {map_path}')
    args.rtt_address = int(match.group(1), 16)
args.log.parent.mkdir(parents=True, exist_ok=True)
lib = pylink.library.Library(dllpath=r'C:\Program Files\SEGGER\JLink_V982\JLink_x64.dll')
with pylink.JLink(lib=lib) as probe:
    probe.open(serial_no=args.serial)
    probe.set_tif(pylink.enums.JLinkInterfaces.SWD)
    probe.connect('NRF54L15_M33', speed=1000)
    print(f'Connected SN={args.serial}, VTref={probe.hardware_status.VTarget} mV', flush=True)
    if args.backup:
        if args.backup.exists():
            raise FileExistsError(args.backup)
        probe.halt()
        args.backup.parent.mkdir(parents=True, exist_ok=True)
        data = bytearray()
        for addr in range(0, 0x17D000, 4096):
            chunk = probe.memory_read8(addr, min(4096, 0x17D000 - addr))
            if len(chunk) != min(4096, 0x17D000 - addr):
                raise IOError(f'Incomplete backup read at {addr:#x}')
            data.extend(chunk)
        args.backup.write_bytes(data)
        print(f'Original RRAM saved to {args.backup}', flush=True)
    if args.hex:
        probe.halt()
        probe.flash_file(str(args.hex.resolve()), 0)
        print(f'Programmed {args.hex}', flush=True)
        probe.reset(halt=True)
        # RTT lives in no-init RAM. A different image can otherwise inherit
        # old buffer pointers/indices even when the control-block address matches.
        if args.rtt_address is not None:
            probe.memory_write8(args.rtt_address, [0] * 16)
    probe.rtt_start(args.rtt_address)
    probe.restart()
    deadline = time.monotonic() + args.seconds
    next_refresh = time.monotonic() + args.rtt_refresh
    command_time = time.monotonic() + 1
    pending_commands = list(args.command)
    with args.log.open('w', encoding='utf-8') as output:
        while time.monotonic() < deadline:
            if args.rtt_refresh > 0 and time.monotonic() >= next_refresh:
                probe.rtt_stop()
                probe.rtt_start(args.rtt_address)
                next_refresh = time.monotonic() + args.rtt_refresh
            if pending_commands and time.monotonic() >= command_time:
                probe.rtt_write(0, (pending_commands.pop(0) + '\n').encode())
                command_time = time.monotonic() + 1
            try:
                data = bytes(probe.rtt_read(0, 8192))
            except pylink.errors.JLinkRTTException:
                time.sleep(0.1)
                continue
            if data:
                value = data.decode('utf-8', errors='replace')
                output.write(value)
                output.flush()
                print(value, end='', flush=True)
            time.sleep(0.02)
    probe.rtt_stop()
