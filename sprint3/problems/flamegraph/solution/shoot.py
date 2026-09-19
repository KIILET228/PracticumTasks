import argparse
import shlex
import signal
import subprocess
import time
import random

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 100
COOLDOWN = 0.1

# Файлы, в которые складываются промежуточные и итоговые артефакты профилирования.
PERF_DATA_FILE = 'perf.data'
GRAPH_FILE = 'graph.svg'
# Каталог с Perl-скриптами FlameGraph должен лежать рядом со скриптом.
FLAMEGRAPH_DIR = 'FlameGraph'

# Время, которое даём серверу на старт и perf record - на подключение к процессу.
STARTUP_DELAY = 1
ATTACH_DELAY = 1


def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str)
    return parser.parse_args().server


def run(command, output=None):
    process = subprocess.Popen(shlex.split(command), stdout=output, stderr=subprocess.DEVNULL)
    return process


def stop(process, wait=False):
    if process.poll() is None and wait:
        process.wait()
    process.terminate()


def shoot(ammo):
    hit = run('curl ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')


def start_perf_record(pid):
    # Пишем данные явно в PERF_DATA_FILE через -o, как рекомендовано в задании,
    # чтобы поведение не зависело от того, как запущен сам shoot.py.
    # -g включает запись call-graph (стека вызовов) - без этого будет
    # трасса без вложенности и во флеймграфе не будет видно, что кто кого вызывает,
    # в частности не будет видно методов RequestHandler.
    command = f'perf record -o {PERF_DATA_FILE} -p {pid} -g'
    return subprocess.Popen(shlex.split(command), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def stop_perf_record(perf_process):
    # perf record должен быть остановлен "мягко" (аналог Ctrl+C),
    # чтобы он успел корректно дописать/закрыть perf.data.
    # kill -9 или terminate() (SIGTERM) может привести к повреждённому файлу.
    perf_process.send_signal(signal.SIGINT)
    perf_process.wait()


def build_flamegraph():
    # "Двойной пайп": perf script -> stackcollapse-perf.pl -> flamegraph.pl > graph.svg
    with open(GRAPH_FILE, 'w') as graph_file:
        perf_script = subprocess.Popen(
            shlex.split(f'perf script -i {PERF_DATA_FILE}'),
            stdout=subprocess.PIPE,
        )
        stackcollapse = subprocess.Popen(
            shlex.split(f'{FLAMEGRAPH_DIR}/stackcollapse-perf.pl'),
            stdin=perf_script.stdout,
            stdout=subprocess.PIPE,
        )
        # Закрываем на своей стороне, чтобы perf_script получил SIGPIPE,
        # когда stackcollapse-perf.pl закончит читать (стандартная идиома для пайпов в subprocess).
        perf_script.stdout.close()

        flamegraph = subprocess.Popen(
            shlex.split(f'{FLAMEGRAPH_DIR}/flamegraph.pl'),
            stdin=stackcollapse.stdout,
            stdout=graph_file,
        )
        stackcollapse.stdout.close()

        flamegraph.wait()
        stackcollapse.wait()
        perf_script.wait()


# 1. Запускаем сервер в фоновом процессе.
server = run(start_server())
time.sleep(STARTUP_DELAY)

# 2. Запускаем perf record для процесса сервера.
perf = start_perf_record(server.pid)
time.sleep(ATTACH_DELAY)

# 3. Обстреливаем сервер запросами.
make_shots()

# 4. Корректно останавливаем perf record, чтобы perf.data не оказался повреждён.
stop_perf_record(perf)

# Сервер больше не нужен - останавливаем его.
stop(server)

# 5. Строим флеймграф.
build_flamegraph()

time.sleep(1)
print('Job done')
