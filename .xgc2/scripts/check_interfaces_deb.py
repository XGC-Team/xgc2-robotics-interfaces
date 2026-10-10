#!/usr/bin/env python3
"""Check the one interfaces Deb after dpkg installation; never rebuild package bytes."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--deb', required=True, type=Path)
    parser.add_argument('--work-dir', required=True, type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2]
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=False)
    checks = []

    def run(*command, success=True):
        command = [str(item) for item in command]
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        with (work / 'commands.log').open('a') as log:
            log.write(json.dumps(command) + '\n' + result.stdout + '\n')
        require((result.returncode == 0) == success,
                'unexpected exit {}: {}\n{}'.format(result.returncode, command, result.stdout))
        return result.stdout.strip()

    metadata = (source / '.xgc2/product.yml').read_text()
    version = re.search(r'^    focal: (\S+)$', metadata, re.M).group(1)
    source_version = version.split('-', 1)[0]
    package = 'libxgc2-robotics-interfaces-dev'
    fields = {field: run('dpkg-deb', '-f', args.deb, field)
              for field in ['Package', 'Version', 'Architecture', 'Depends']}
    require(fields == {'Package': package, 'Version': version, 'Architecture': 'all', 'Depends': ''},
            'unexpected interfaces package fields: ' + str(fields))
    require(run('dpkg-query', '-W', '-f=${Status}', package) == 'install ok installed', 'interfaces not installed')
    require(run('dpkg-query', '-W', '-f=${Version}', package) == version, 'installed interfaces version mismatch')
    consumer = work / 'consumer'
    shutil.copytree(source / 'test', consumer)
    canonical = {path.name: path for path in (source / 'include/xgc-robotics-interfaces').glob('*.h')}
    require(set(canonical) == {'robotics_interfaces_v1.h', 'control_records_v1.h', 'paired_state_v1.h'},
            'unexpected canonical interface headers')
    forbidden = ['xgc_fcu_request_v2', 'xgc_fcu_result_v1', 'xgc_fcu_extended_state_v1',
                 'xgc_sim_provider_request_v1', 'xgc_sim_provider_result_v1']
    texts = [path.read_text() for path in canonical.values()]
    require(sum(len(re.findall(r'typedef struct\s+xgc_', text)) for text in texts) == 14,
            'generic interface record count changed')
    require(not any(name in text for name in forbidden for text in texts),
            'simulation domain leaked into generic interfaces')
    config_names = {'XgcRoboticsInterfacesConfig.cmake', 'XgcRoboticsInterfacesConfigVersion.cmake', 'XgcRoboticsInterfacesTargets.cmake'}
    contracts = {path.name: path for path in (source / 'contracts').glob('*.md')}
    require(set(contracts) == {'simulation-v1.md'}, 'unexpected public contracts')

    def payload(prefix):
        headers = prefix / 'include/xgc-robotics-interfaces'
        require({path.name for path in headers.iterdir()} == set(canonical), 'interfaces header set mismatch')
        for name, original in canonical.items():
            require((headers / name).read_bytes() == original.read_bytes(), 'noncanonical interfaces header: ' + name)
        for name, original in contracts.items():
            require((prefix / 'share/xgc2-robotics-interfaces/contracts' / name).read_bytes() == original.read_bytes(), 'noncanonical contract: ' + name)
        configs = prefix / 'share/cmake/XgcRoboticsInterfaces'
        require({path.name for path in configs.iterdir()} == config_names, 'interfaces CMake package set mismatch')
        for path in configs.iterdir():
            require(str(source) not in path.read_text(), 'installed config leaks source path')

    def consume(name, prefix, build_success=True):
        build = work / name
        run('cmake', '-S', consumer, '-B', build,
            '-DINTERFACES_PREFIX=' + str(prefix), '-DINTERFACES_VERSION=' + source_version)
        run('cmake', '--build', build, '--parallel', '1', success=build_success)
        if build_success:
            run(build / 'c_smoke')
            run(build / 'cxx_smoke')

    extracted = work / 'extracted'
    run('dpkg-deb', '-x', args.deb, extracted)
    expected = {'usr/include/xgc-robotics-interfaces/' + name for name in canonical}
    expected.update('usr/share/cmake/XgcRoboticsInterfaces/' + name for name in config_names)
    expected.update('usr/share/xgc2-robotics-interfaces/contracts/' + name for name in contracts)
    actual = {path.relative_to(extracted).as_posix() for path in extracted.rglob('*') if not path.is_dir()}
    expected.add('usr/share/doc/' + package + '/copyright')
    require(actual == expected, 'interfaces Deb contains missing or extra payload: ' + str(actual ^ expected))
    payload(extracted / 'usr')
    payload(Path('/usr'))
    for path in sorted(expected):
        owner = run('dpkg-query', '-S', '/' + path)
        require(owner.startswith(package + ': '), 'interfaces install ownership mismatch: ' + owner)
    consume('installed', Path('/usr'))
    relocated = work / 'relocated'
    shutil.copytree(extracted / 'usr', relocated)
    consume('relocated', relocated)
    # Remove /usr/include fallback before testing missing relocated payload.
    run('dpkg', '--remove', package)
    consume('relocated-without-installed-fallback', relocated)
    run('cmake', '-S', consumer, '-B', work / 'wrong-version',
        '-DINTERFACES_PREFIX=' + str(relocated), '-DINTERFACES_VERSION=99.0.0', success=False)
    for name in sorted(canonical):
        header = relocated / 'include/xgc-robotics-interfaces' / name
        original = header.read_bytes()
        header.unlink()
        try:
            consume('missing-' + name.replace('.', '-'), relocated, build_success=False)
        finally:
            header.write_bytes(original)
    config = relocated / 'share/cmake/XgcRoboticsInterfaces/XgcRoboticsInterfacesConfig.cmake'
    config.unlink()
    run('cmake', '-S', consumer, '-B', work / 'missing-config',
        '-DINTERFACES_PREFIX=' + str(relocated), '-DINTERFACES_VERSION=' + source_version, success=False)
    run('cmake', '-S', consumer, '-B', work / 'package-removed',
        '-DINTERFACES_PREFIX=/usr', '-DINTERFACES_VERSION=' + source_version, success=False)
    checks.extend(['installed package removal refused by required CMake import', 'canonical header-only Deb payload and installed dpkg ownership',
                   'installed/relocated XgcRoboticsInterfaces::Interfaces C11/C++14 ABI and config smoke',
                   'wrong interfaces version, each missing header and missing package config refused'])
    print(json.dumps({'checks': checks, 'package_fields': fields,
                      'deb_sha256': hashlib.sha256(args.deb.read_bytes()).hexdigest(),
                      'architecture': run('dpkg', '--print-architecture')}, indent=2))


if __name__ == '__main__':
    main()
