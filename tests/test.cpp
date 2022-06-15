#include <cassert>
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <lib.h>
#include <minix/rs.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

std::vector<int> get_order(std::vector<std::function<void ()>> thread_functions) {
	size_t n = thread_functions.size();

	std::vector<int> pids(n);
	for (size_t i = 0; i < n; ++i) {
		pids[i] = fork();
		assert(pids[i] >= 0);
		if (pids[i] == 0) {
			thread_functions[i]();
			_exit(0);
		}
	}
	int killer_pid = fork();
	if (killer_pid == 0) {
		sleep(1);
		for (size_t i = 0; i < n; ++i)
			kill(pids[i], SIGTERM);
		_exit(0);
	}

	std::vector<int> order;
	for (int i = 0; i < n + 1; ++i) {
		int wstatus;
		int child_pid = wait(&wstatus);
		assert(child_pid != -1);
		if (child_pid == killer_pid)
			continue;

		for (int j = 0; j < n; ++j)
			if (child_pid == pids[j])
				order.emplace_back(j);
	}
	assert(order.size() == n);
	return order;
}

extern "C" {
	int notify(int fd, int type);
}

void sleep_units(int cnt) {
	std::this_thread::sleep_for(std::chrono::milliseconds(100 * cnt));
}

std::function<void ()> just_sleep_open_sleep(std::string path, int flags = O_RDONLY) {
	return [=] {
		sleep_units(5);
		int fd = open(path.c_str(), flags);
		assert(fd != -1);
		sleep_units(1);
		close(fd);
	};
}

std::function<void ()> just_notify_sleep(std::string path, int notify_type) {
	return [=] {
		int fd = open(path.c_str(), O_RDONLY);
		assert(fd != -1);
		sleep_units(1);
		assert(notify(fd, notify_type) == 0);
		sleep_units(2);
		close(fd);
	};
}

const std::vector<std::string> readonly_files = {
	"files/file.txt",
	"files/file_hard.txt",
	"files/file_soft.txt",
	"/dev/zero",
	"/proc/cpuinfo",
};

void test_notify_open() {
	for (const std::string &path : readonly_files) {
		std::cerr << "notify open " << path << "...\n";
		assert(
			get_order({
				just_notify_sleep(path, NOTIFY_OPEN),
				just_sleep_open_sleep(path)
			}) == (std::vector<int>{1, 0})
		);
	}

	std::cerr << "multiple notify open...\n";
	assert(
		get_order({
			just_notify_sleep(readonly_files[0], NOTIFY_OPEN),
			just_notify_sleep(readonly_files[0], NOTIFY_OPEN),
			just_notify_sleep(readonly_files[0], NOTIFY_OPEN),
			just_notify_sleep(readonly_files[0], NOTIFY_OPEN),
			just_sleep_open_sleep(readonly_files[0]),
		})[0] == 4
	);

	std::cerr << "more multiple notify open...\n";
	std::function<void ()> notify_expect_fail = [&] {
		int fd = open(readonly_files[0].c_str(), O_RDONLY);
		assert(fd != -1);
		sleep_units(2);
		assert(notify(fd, NOTIFY_OPEN) == ENONOTIFY);
		close(fd);
	};
	std::vector<int> order = get_order({
		notify_expect_fail,
		notify_expect_fail,
		notify_expect_fail,
		notify_expect_fail,
		just_notify_sleep(readonly_files[0], NOTIFY_OPEN),
		just_notify_sleep(readonly_files[0], NOTIFY_OPEN),
		just_notify_sleep(readonly_files[0], NOTIFY_OPEN),
		just_notify_sleep(readonly_files[0], NOTIFY_OPEN),
		just_notify_sleep(readonly_files[0], NOTIFY_OPEN),
		just_notify_sleep(readonly_files[0], NOTIFY_OPEN),
		just_notify_sleep(readonly_files[0], NOTIFY_OPEN),
		just_notify_sleep(readonly_files[0], NOTIFY_OPEN),
		just_sleep_open_sleep(readonly_files[0]),
		just_sleep_open_sleep(readonly_files[0]),
		just_sleep_open_sleep(readonly_files[0]),
	});
	for (int i = 0; i < 4; ++i)
		assert(0 <= order[i] and order[i] < 4);
	for (int i = 4; i < 7; ++i)
		assert(12 <= order[i] and order[i] < 15);
	for (int i = 7; i < 15; ++i)
		assert(4 <= order[i] and order[i] < 12);

	std::cerr << "notify wrong fd...\n";
	int fd = open(readonly_files[0].c_str(), O_RDONLY);
	close(fd);
	assert(notify(fd, NOTIFY_OPEN) == EBADF);
}

void test_notify_triopen() {
	for (const std::string &path : readonly_files) {
		std::cerr << "notify triopen 1 " << path << "...\n";
		assert(
			get_order({
				just_notify_sleep(path, NOTIFY_TRIOPEN),
				just_sleep_open_sleep(path),
				[&] { sleep_units(8); }
			}) == (std::vector<int>{1, 2, 0})
		);

		if (path == "/dev/zero" or path == "/proc/cpuinfo")
			continue; // ?

		std::cerr << "notify triopen 2 " << path << "...\n";
		assert(
			get_order({
				just_notify_sleep(path, NOTIFY_TRIOPEN),
				just_sleep_open_sleep(path),
				[&] { 
					int fd = open(readonly_files[0].c_str(), O_RDONLY);
					sleep_units(8);
					close(fd);
				}
			}) == (std::vector<int>{1, 0, 2})
		);

		std::vector<int> order = get_order({
			just_notify_sleep(path, NOTIFY_TRIOPEN),
			just_notify_sleep(path, NOTIFY_TRIOPEN),
			just_notify_sleep(path, NOTIFY_TRIOPEN),
			just_sleep_open_sleep(path),
		});
		assert(order[3] == 3);
	}
}

void test_notify_create() {
	std::cerr << "notify create...\n";
	get_order({[&] {
		int fd = open(readonly_files[0].c_str(), O_RDONLY);
		assert(notify(fd, NOTIFY_CREATE) == ENOTDIR);
		close(fd);
	}});

	const std::string dir = "files/create/";
	assert(
		get_order({
			just_notify_sleep(dir, NOTIFY_CREATE),
			[&] { sleep_units(8); }
		}) == (std::vector<int>{1, 0})
	);

	assert(
		get_order({
			just_notify_sleep(dir, NOTIFY_CREATE),
			just_sleep_open_sleep(dir + "create.txt", O_CREAT),
			[&] { sleep_units(8); }
		}) == (std::vector<int>{1, 0, 2})
	);

    assert(
        get_order({
            just_notify_sleep(dir, NOTIFY_CREATE),
			[=] {
				std::string path = dir + "special_mknod";
				sleep_units(5);
				int fd = mknod(path.c_str(), 777 | S_IFREG, 0x0201);
				assert(fd != -1);
				sleep_units(1);
				close(fd);
			},
            [&] { sleep_units(8); }
        }) == (std::vector<int>{1, 0, 2})
    );

    assert(
        get_order({
            just_notify_sleep(dir, NOTIFY_CREATE),
			[=] {
				std::string path = dir + "folder_mkdir";
				sleep_units(5);
				int fd = mkdir(path.c_str(), 777);
				assert(fd != -1);
				sleep_units(1);
				close(fd);
			},
            [&] { sleep_units(8); }
        }) == (std::vector<int>{1, 0, 2})
    );

	assert(
		get_order({
			just_notify_sleep(dir, NOTIFY_CREATE),
			just_sleep_open_sleep(dir, O_CREAT),
			[&] { sleep_units(8); }
		}) == (std::vector<int>{1, 2, 0})
	);

	assert(
		get_order({
			just_notify_sleep(dir, NOTIFY_CREATE),
			just_sleep_open_sleep(dir + "inside/create.txt", O_CREAT),
			[&] { sleep_units(8); }
		}) == (std::vector<int>{1, 2, 0})
	);

    std::cerr << "notify create symlinks and hardlinks...\n";

    assert(
        get_order({
            just_notify_sleep(dir, NOTIFY_CREATE),
			[=] {
				sleep_units(5);
				std::string path = dir + "group_sym";
				int ret = symlink("/etc/group", path.c_str());
				assert(ret != -1);
				sleep_units(1);
			},
            [&] { sleep_units(8); }
        }) == (std::vector<int>{1, 0, 2})
    );

    assert(
        get_order({
            just_notify_sleep(dir, NOTIFY_CREATE),
			[=] {
				sleep_units(5);
				std::string path = dir + "group_hard";
				int ret = link("/etc/group", path.c_str());
				assert(ret != -1);
				sleep_units(1);
			},
            [&] { sleep_units(8); }
        }) == (std::vector<int>{1, 0, 2})
    );

}

void test_notify_move() {
	std::cerr << "notify move..." << std::endl;
	get_order({[&] {
		int fd = open(readonly_files[0].c_str(), O_RDONLY);
		assert(notify(fd, NOTIFY_MOVE) == ENOTDIR);
		close(fd);
	}});

	const std::string dir1 = "files/move1/";
	const std::string dir2 = "files/move2/";
	assert(
		get_order({
			just_notify_sleep(dir1, NOTIFY_MOVE),
			[&] { sleep_units(8); }
		}) == (std::vector<int>{1, 0})
	);

	auto just_rename = [&](std::string from, std::string to) {
		return [=] {
			sleep_units(5);
			assert(rename(from.c_str(), to.c_str()) == 0);
			sleep_units(1);
		};
	};
	assert(
		get_order({
			just_notify_sleep(dir1, NOTIFY_MOVE),
			just_rename(dir1 + "move.txt", dir2 + "move.txt"),
			[&] { sleep_units(8); }
		}) == (std::vector<int>{1, 2, 0})
	);

	assert(
		get_order({
			just_notify_sleep(dir1, NOTIFY_MOVE),
			just_rename(dir2 + "move.txt", dir1 + "move.txt"),
			[&] { sleep_units(8); }
		}) == (std::vector<int>{1, 0, 2})
	);

	assert(
		get_order({
			just_notify_sleep(dir1, NOTIFY_MOVE),
			just_rename(dir1 + "move.txt", dir1 + "move2.txt"),
			[&] { sleep_units(8); }
		}) == (std::vector<int>{1, 2, 0})
	);

	assert(
		get_order({
			just_notify_sleep(dir1, NOTIFY_MOVE),
			just_sleep_open_sleep(dir2 + "create.txt", O_CREAT),
			[&] { sleep_units(8); }
		}) == (std::vector<int>{1, 2, 0})
	);

	assert(
		get_order({
			just_notify_sleep(dir1, NOTIFY_MOVE),
			just_sleep_open_sleep(dir2 + "create.txt"),
			[&] { sleep_units(8); }
		}) == (std::vector<int>{1, 2, 0})
	);
}

void test_notify_argument() {
	std::cerr << "notify arguments...\n";
	int fd = open(readonly_files[0].c_str(), O_RDONLY);
	assert(notify(fd, NOTIFY_OPEN - 1) == EINVAL);
	assert(notify(fd, NOTIFY_MOVE + 1) == EINVAL);
	close(fd);
}

void test_nr_notify() {
	/*
		Zapełniamy NR_NOTIFY slotów notify i patrzymy czy następne wywołanie kończy się ENONOTIFY.
		Potem ubijamy te procesy. Patrzymy czy sloty są z powrotem dostępne.
	*/
	std::cerr << "notify NR_NOTIFY...\n";
	
	const int nr_notify = 8;

	const std::vector<std::string> file_names = {
		"files/nr1",
		"files/nr2",
		"files/nr3",
		"files/nr4",
		"files/nr5",
		"files/nr6",
		"files/nr7",
		"files/nr8",
		"files/nr9_enonotify",
	};

	for(auto& filename : file_names)
	{
		int fd = mkdir(filename.c_str(), 777);
		assert(fd != -1);
	}
	
	// Jak ktoś ma mniej to można tu zamienić
	// std::vector<int> events = {NOTIFY_OPEN, NOTIFY_OPEN, NOTIFY_OPEN, NOTIFY_OPEN};
	std::vector<int> events = {NOTIFY_OPEN, NOTIFY_TRIOPEN, NOTIFY_MOVE, NOTIFY_CREATE};
	for(int event : events) 
	{
		int pids[8];
		for(int i = 0; i < nr_notify; i++) 
		{
			int pid = fork();
			if(pid < 0)
			{
				std::cerr << "Error in fork\n";
				_exit(1);
			}
			else if (pid == 0)
			{
				int fd = open(file_names[i].c_str(), O_RDONLY);
				assert(fd != -1);
				assert(notify(fd, event) == 0);
				close(fd);
				_exit(0);
			} 
			else
			{
				pids[i] = pid;

				if(i == nr_notify - 1)
				{
					sleep_units(3);
					int fd = open(file_names[nr_notify].c_str(), O_RDONLY | O_CREAT);
					assert(notify(fd, event) == ENONOTIFY);
					close(fd);

					for(int j = 0; j < nr_notify; j++)
					{
						kill(pids[j], SIGINT);
						wait(0);
					}
				}
			}
		}
	}


	_exit(0);
}

int main() {
	test_notify_open();
	test_notify_triopen();
	test_notify_create();
	test_notify_move();
	test_notify_argument();
	test_nr_notify();
}
