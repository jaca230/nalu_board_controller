#!/usr/bin/env python3
"""Firmware 959 normal/WLC deadtime scan without MIDAS."""
import argparse, itertools, json, math, os, subprocess, sys, time
from datetime import datetime, timezone
from pathlib import Path
APP_DIR = Path(__file__).resolve().parents[1]

def append_jsonl(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('a', encoding='utf-8') as stream:
        stream.write(json.dumps(value, separators=(',', ':')) + '\n'); stream.flush(); os.fsync(stream.fileno())

def completed_records(path):
    records=[]
    if path.exists():
        for line in path.read_text().splitlines():
            try: records.append(json.loads(line))
            except json.JSONDecodeError: pass
    return records

class Pico:
    def __init__(self, config):
        sys.path.insert(0, str(Path(config['source_directory'])))
        from serial_controller import SerialController
        from serial_command_sender import SerialCommandSender
        self.serial=SerialController(port=config['serial_port'], debug=False); self.serial.connect()
        self.sender=SerialCommandSender(self.serial); self.config=config; self.hardware_id=config['hardware_id']
        self.current_rate=None; self.current_separation=None; self.running=False
    def configure(self, rate, separation):
        if self.current_rate != rate:
            for command in ('stop','delete'):
                try: self.sender.send_command(command, self.hardware_id)
                except Exception: pass
            settings={'pin_number':self.config['pin_number'],'repetition_rate_hz':rate,
                      'pulse_width_ns':self.config['pulse_width_ns'],'separation_ns':separation,
                      'start_on_init':False}
            self.sender.send_command('create','double_pulse',settings,self.hardware_id)
            self.current_rate=rate; self.current_separation=separation
        elif self.current_separation != separation:
            self.sender.send_command('apply_hardware_settings',self.hardware_id,{'separation_ns':separation})
            self.current_separation=separation
        if not self.running:
            self.sender.send_command('start',self.hardware_id); self.running=True
    def stop(self):
        if self.running: self.sender.send_command('stop',self.hardware_id); self.running=False
    def close(self):
        try: self.stop()
        finally: self.serial.close()

class BoardProcess:
    def __init__(self, executable, config_path):
        self.process=subprocess.Popen([str(executable),'--config',str(config_path),'--serve'],
                                      stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True,bufsize=1)
    def measure(self, wlc, requested, channels, duration):
        request={'wlc_enabled':wlc,'requested_windows':requested,'active_channels':channels,
                 'duration_seconds':duration}
        self.process.stdin.write(json.dumps(request)+'\n'); self.process.stdin.flush()
        for line in self.process.stdout:
            if line.startswith('RESULT_JSON:'): return json.loads(line[len('RESULT_JSON:'):])
            print(line,end='')
        raise RuntimeError(f'Board process exited with {self.process.poll()}')
    def close(self):
        if self.process.poll() is not None:
            return
        try:
            if self.process.stdin: self.process.stdin.close()
            self.process.wait(timeout=5)
        except (subprocess.TimeoutExpired, BrokenPipeError):
            self.process.terminate()
            try: self.process.wait(timeout=5)
            except subprocess.TimeoutExpired: self.process.kill(); self.process.wait()

def wlc_window_mapping(desired, enabled):
    if not enabled: return desired, desired
    requested=max(1,min(12,desired-2))
    return requested, 2*((requested+1)//2)+2

def dry_result(enabled, requested, channels, duration, returned):
    return {'wlc_enabled':enabled,'requested_windows':requested,'active_channels':channels,
            'duration_seconds':duration,'observed_duration_seconds':duration,'packets_total':0,
            'events_total':0,'packets_per_second':0.0,'events_per_second':0.0,'dry_run':True,
            'hardware_reconfigured':True,'logical_positions':{str(i):0 for i in range(returned)}}

def format_duration(seconds):
    if seconds is None: return 'estimating'
    seconds=max(0,int(round(seconds))); hours,remainder=divmod(seconds,3600); minutes,seconds=divmod(remainder,60)
    if hours: return f'{hours}h {minutes:02d}m'
    if minutes: return f'{minutes}m {seconds:02d}s'
    return f'{seconds}s'

def valid_measurement(record, maximum_ratio):
    ratio=record.get('packet_rates',{}).get('ratio_vs_single_pulse')
    return (record.get('measurement_status','valid')=='valid' and
            record.get('listener',{}).get('events_total',0)>0 and
            isinstance(ratio,(int,float)) and 0<=ratio<=maximum_ratio)

def search_measurement(record, maximum_ratio):
    return (valid_measurement(record,maximum_ratio) or
            record.get('measurement_status')=='failed_after_retries')

def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--config',type=Path,default=APP_DIR/'config.json')
    parser.add_argument('--dry-run',action='store_true'); args=parser.parse_args()
    config_path=args.config.resolve(); config=json.loads(config_path.read_text())
    executable=APP_DIR/'build/bin/firmware_959_deadtime_point'
    if not args.dry_run and not executable.exists(): raise SystemExit(f'Build first: {APP_DIR}/scripts/build.sh')
    output=Path(config['logging']['output'])
    if not output.is_absolute(): output=(APP_DIR/output).resolve()
    existing=completed_records(output); pico=None if args.dry_run else Pico(config['pico'])
    board=None if args.dry_run else BoardProcess(executable,config_path)
    search,run,space=config['binary_search'],config['run'],config['parameter_space']; combo_index=0
    combinations=list(itertools.product(
        space['wlc_enabled'],space['channels'],space['windows'],space['pulse_rates_hz']))
    total_parameters=len(combinations)
    maximum_ratio=float(search.get('maximum_valid_ratio',2.2))
    start_ns=int(search.get('start_ns',(search['min_ns']+search['max_ns'])//2))
    if not search['min_ns']<=start_ns<=search['max_ns']:
        raise ValueError('binary_search.start_ns must be inside the search range')
    largest_after_start=max(start_ns-search['min_ns'],search['max_ns']-start_ns)
    search_iterations=min(search['max_iterations'],1+math.ceil(math.log2(
        max(1,largest_after_start)/search['tolerance_ns'])))
    total_measurements=total_parameters*search_iterations
    completed_before=0
    for enabled,channel_set,desired,rate in combinations:
        mode='wlc' if enabled else 'normal'; _,returned=wlc_window_mapping(desired,enabled)
        key={'rate_hz':rate,'desired_windows':desired,'returned_windows':returned,
             'mode':mode,'channel_set':channel_set['id'],'metric':'single_pulse_packet_ratio_v1'}
        completed_before+=min(search_iterations,sum(
            1 for r in existing if r.get('scan_key')==key and search_measurement(r,maximum_ratio)))
    measured_this_run=0; measurement_seconds=0.0
    try:
        for enabled,channel_set,desired,rate in combinations:
            combo_index+=1; mode='wlc' if enabled else 'normal'
            requested,returned=wlc_window_mapping(desired,enabled); channels=channel_set['channels']
            low,high=search['min_ns'],search['max_ns']
            key={'rate_hz':rate,'desired_windows':desired,'returned_windows':returned,
                 'mode':mode,'channel_set':channel_set['id'],
                 'metric':'single_pulse_packet_ratio_v1'}
            prior=[r for r in existing if r.get('scan_key')==key and search_measurement(r,maximum_ratio)]
            for record in prior:
                separation=record['double_pulse']['separation_ns']
                if record['search']['passed']: high=min(high,separation)
                else: low=max(low,separation)
            iteration=len(prior)
            while high-low>search['tolerance_ns'] and iteration<search['max_iterations']:
                separation=start_ns if iteration==0 else (low+high)//2
                point_started=time.monotonic()
                average=(measurement_seconds/measured_this_run if measured_this_run else
                         run['duration_seconds']+run['settle_seconds']+4)
                parameter_remaining=max(0,search_iterations-iteration)
                total_remaining=max(0,total_measurements-completed_before-measured_this_run)
                print(f"[parameter {combo_index}/{total_parameters}] "
                      f"[search {iteration+1}/{search_iterations}] starting {mode} "
                      f"channels={channel_set['id']} windows={desired}; "
                      f"parameter ETA {format_duration(parameter_remaining*average)}, "
                      f"total ETA {format_duration(total_remaining*average)}", flush=True)
                retry_count=0
                max_retries=int(run.get('zero_event_max_retries',0))
                expected_single=rate*returned*len(channels)
                expected_double=2.0*expected_single
                retries_exhausted=False
                while True:
                    if pico: pico.configure(rate,separation); time.sleep(run['settle_seconds'])
                    try:
                        point=(dry_result(enabled,requested,channels,run['duration_seconds'],returned)
                               if args.dry_run else board.measure(
                                   enabled,requested,channels,run['duration_seconds']))
                    finally:
                        if pico: pico.stop()
                    attempt_ratio=(point['packets_per_second']/expected_single
                                   if expected_single else 0.0)
                    invalid_reason=None
                    if not args.dry_run and point.get('events_total',0)==0:
                        invalid_reason='zero_events'
                    elif not args.dry_run and not 0<=attempt_ratio<=maximum_ratio:
                        invalid_reason='implausible_ratio'
                    if invalid_reason is None:
                        break
                    invalid_record={
                      'timestamp':datetime.now(timezone.utc).isoformat(),
                      'measurement_status':f'invalid_{invalid_reason}','scan_key':key,
                      'double_pulse':{'repetition_rate_hz':rate,'separation_ns':separation},
                      'capture_settings':{'mode':mode,'wlc_enabled':enabled,
                        'desired_windows':desired,'requested_windows':requested,
                        'expected_returned_windows':returned,'active_channels':channels,
                        'channel_set':channel_set['id']},
                      'listener':point,
                      'packet_rates':{'ratio_vs_single_pulse':attempt_ratio,
                        'maximum_valid_ratio':maximum_ratio},
                      'retry':{'failed_attempt':retry_count+1,'max_retries':max_retries,
                        'exhausted':retry_count>=max_retries},
                      'config':config}
                    append_jsonl(output,invalid_record)
                    if retry_count>=max_retries:
                        retries_exhausted=True
                        print(f"Invalid measurement ({invalid_reason}) after "
                              f"{max_retries+1} attempts; marking this search point failed",
                              file=sys.stderr,flush=True)
                        break
                    retry_count+=1
                    delay=float(run.get('zero_event_retry_delay_seconds',0))
                    print(f"Invalid measurement ({invalid_reason}); retry "
                          f"{retry_count}/{max_retries} "
                          f"after {delay:g}s",flush=True)
                    if delay: time.sleep(delay)
                if retries_exhausted:
                    failed_record={
                      'timestamp':datetime.now(timezone.utc).isoformat(),
                      'measurement_status':'failed_after_retries','scan_key':key,
                      'double_pulse':{'repetition_rate_hz':rate,'separation_ns':separation},
                      'capture_settings':{'mode':mode,'wlc_enabled':enabled,
                        'desired_windows':desired,'requested_windows':requested,
                        'expected_returned_windows':returned,'active_channels':channels,
                        'channel_set':channel_set['id']},
                      'listener':point,
                      'search':{'combo_index':combo_index,'iteration':iteration+1,
                        'low_ns':low,'high_ns':high,'threshold':search['ratio_threshold'],
                        'passed':False,'failure_reason':'invalid_measurement_retries_exhausted'},
                      'config':config}
                    append_jsonl(output,failed_record)
                    measured_this_run+=1; measurement_seconds+=time.monotonic()-point_started
                    print(f"[parameter {combo_index}/{total_parameters}] "
                          f"[search {iteration+1}/{search_iterations}] point failed after retries; "
                          f"continuing this parameter search",flush=True)
                    low=separation
                    iteration+=1
                    continue
                ratio=point['packets_per_second']/expected_single if expected_single else 0.0
                if args.dry_run: ratio=2.0 if separation>=100000 else 1.0
                passed=ratio>=search['ratio_threshold']
                record={'timestamp':datetime.now(timezone.utc).isoformat(),
                  'measurement_status':'valid','retry_count':retry_count,'scan_key':key,
                  'double_pulse':{'repetition_rate_hz':rate,'separation_ns':separation},
                  'capture_settings':{'mode':mode,'wlc_enabled':enabled,'desired_windows':desired,
                    'requested_windows':requested,'expected_returned_windows':returned,
                    'active_channels':channels,'channel_set':channel_set['id']},
                  'packet_rates':{'observed_packets_per_sec':point['packets_per_second'],
                    'expected_single_pulse_packets_per_sec':expected_single,
                    'expected_double_pulse_packets_per_sec':expected_double,
                    'ratio_vs_single_pulse':ratio,
                    'packets_total':point['packets_total']},
                  'observed_rates':{'events_per_second':point['events_per_second'],'events_total':point['events_total']},
                  'listener':point,'search':{'combo_index':combo_index,'iteration':iteration+1,
                    'low_ns':low,'high_ns':high,'threshold':search['ratio_threshold'],'passed':passed},
                  'config':config}
                append_jsonl(output,record)
                measured_this_run+=1; measurement_seconds+=time.monotonic()-point_started
                average=measurement_seconds/measured_this_run
                total_remaining=max(0,total_measurements-completed_before-measured_this_run)
                print(f"[parameter {combo_index}/{total_parameters}] "
                      f"[search {iteration+1}/{search_iterations}] {mode} "
                      f"channels={channel_set['id']} desired={desired} returned={returned} "
                      f"separation={separation} ns ratio={ratio:.3f} {'PASS' if passed else 'FAIL'}")
                print(f"Progress: {completed_before+measured_this_run}/{total_measurements} measurements; "
                      f"total ETA {format_duration(total_remaining*average)}", flush=True)
                if passed: high=separation
                else: low=separation
                iteration+=1
    except KeyboardInterrupt:
        print('\nInterrupt received; stopping Pico and board capture...', file=sys.stderr)
    finally:
        if pico: pico.close()
        if board: board.close()
    print(f'Results: {output}')
if __name__=='__main__': main()
