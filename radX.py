#!/usr/bin/env python

import time

import click
import serial

global_verbose = False
global_ser = None

def set_freq(clock, freq):
    cmd = f'clk {clock} {freq}\n'
    global_ser.write(bytearray(cmd, 'utf-8'))
    line = global_ser.readline().decode()
    line = global_ser.readline().decode()
    if line.startswith('#'): # and not global_verbose:
        return
    else:
        print(line)


@click.group()
@click.option('--verbose/--quiet', help='should detailed messages be printed', default=False)
@click.option('--dev', help='serial device', default='/dev/ttyACM0')
@click.option('--baud', help='baud rate', default=115200)
@click.option('--timeout', help='timeout in s for serial reads', default=1)
def cli(verbose, dev, baud, timeout):
    global global_ser
    global_ser = serial.Serial(dev, baud, timeout=timeout)

    global global_verbose
    if verbose:
        global_verbose = True
        click.echo('verbose mode on')
    else:
        global_verbose = False


@cli.command()
@click.option('--clock', default=0, help='clock to set [0, 1, 2]')
@click.option('--start', default=6999000, help='start frequency of sweep in Hz')
@click.option('--end', default=7001000, help='end frequency of sweep in Hz')
@click.option('--stride', default=10, help='sweep stride in Hz')
@click.option('--sleep', default=.25, help='settle time in s at each frequency before continuing sweep')
def sweep(clock, start, end, stride, sleep):
    if global_verbose:
        click.echo(f'sweeping clock {clock} from {start} to {end} in {stride} Hz steps, pausing for {sleep} s')
    for freq in range(start, end, stride):
        set_freq(clock, freq)
        time.sleep(sleep)


@cli.command()
@click.option('--clock', default=0, help='clock to set [0, 1, 2]')
@click.option('--freq', default=7000000, help='frequency in Hz')
def clock(clock, freq):
    if global_verbose:
        click.echo(f'setting clock {clock} to {freq} Hz')
    set_freq(clock, freq)


@cli.command()
def reset():
    if global_verbose:
        click.echo('resetting radio')
    with serial.Serial('/dev/ttyACM0', 115200, timeout=1) as ser:
        ser.write(b'rst\n')
    time.sleep(2)
    with serial.Serial('/dev/ttyACM0', 115200, timeout=1) as ser:
        print(ser.readline().decode())
        print(ser.readline().decode())
        print(ser.readline().decode())
        print(ser.readline().decode())


if __name__ == "__main__":
    cli()
