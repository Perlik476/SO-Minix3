#include <lib.h>
#include <minix/rs.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    message m;
    endpoint_t vfs_ep;
    int ret;
    int fd;

    // Sprawdź liczbę argumentów.
    if (argc != 2) {
        printf("Użycie:\n%s plik\n", argv[0]);
        return 1;
    }

    // Otwórz plik.
    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        printf("Nie można otworzyć pliku %s (wynik: %d).\n", argv[1], fd);
        return 1;
    }

    // Wywołaj VFS_NOTIFY.
    m.m_lc_vfs_notify.fd = fd;
    m.m_lc_vfs_notify.event = NOTIFY_OPEN;
    minix_rs_lookup("vfs", &vfs_ep);
    ret = _syscall(vfs_ep, VFS_NOTIFY, &m);
    printf("Wynik VFS_NOTIFY: %d, errno: %d\n", ret, errno);

    // Zamknij plik.
    close(fd);

    // Zakończ.
    return ret != 0;
}
