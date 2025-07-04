// commands.c
#include "os.h"

void cpuid(int code, unsigned int* eax, unsigned int* ebx, unsigned int* ecx, unsigned int* edx) {
    unsigned int a, b, c, d;
    __asm__ volatile (
        "cpuid"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "a"(code)
    );
    *eax = a;
    *ebx = b;
    *ecx = c;
    *edx = d;
}

void get_cpu_vendor(char *out) {
    unsigned int eax, ebx, ecx, edx;
    cpuid(0, &eax, &edx, &ecx, &ebx);
    ((unsigned int*)out)[0] = ebx;
    ((unsigned int*)out)[1] = edx;
    ((unsigned int*)out)[2] = ecx;
    out[12] = '\0';
}

void get_cpu_brand(char *out) {
    unsigned int regs[4];
    for (int i = 0; i < 3; i++) {
        cpuid(0x80000002 + i, &regs[0], &regs[1], &regs[2], &regs[3]);
        ((unsigned int*)out)[i * 4 + 0] = regs[0];
        ((unsigned int*)out)[i * 4 + 1] = regs[1];
        ((unsigned int*)out)[i * 4 + 2] = regs[2];
        ((unsigned int*)out)[i * 4 + 3] = regs[3];
    }
    out[48] = '\0';
}

void neofetch() {
    print_string("guyOS v3.0\n=================\n");

    char cpu_vendor[13];
    get_cpu_vendor(cpu_vendor);
    print_string("CPU Vendor : ");
    print_string(cpu_vendor);
    print_string("\n");

    char cpu_name[49];
    get_cpu_brand(cpu_name);
    print_string("CPU Name   : ");
    print_string(cpu_name);
    print_string("\n");

    print_string("Shell      : guyOS shell\n");
}

void reboot() {
    unsigned char good = 0x02;
    while (good & 0x02) {
        __asm__ __volatile__("inb $0x64, %0" : "=a"(good));
    }
    outb(0x64, 0xFE);
}

void shutdown() {
    __asm__ __volatile__("outw %%ax, %%dx" : : "a"(0x2000), "d"(0x604));
    outb(0xF4, 0x00);
    __asm__ __volatile__("cli; hlt");
    while (1);
}

void run_command(const char* cmd) {
    if (strcmp(cmd, "help") == 0) {
        print_string("Commands:\n");
        print_string("help       -   Displays this help message\n");
        print_string("cls        -   Clears the screen\n");
        print_string("echo       -   Prints the text after the cmd\n");
        print_string("neofetch   -   Shows system info\n");
        print_string("exit       -   Shutdown\n");
        print_string("reboot     -   Reboots the machine\n");
    } else if (strcmp(cmd, "cls") == 0) {
        clear_screen();
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        print_string(cmd + 5);
        print_string("\n");
    } else if (strcmp(cmd, "exit") == 0) {
        print_string("Goodbye!\n");
        shutdown();
    } else if (strcmp(cmd, "reboot") == 0) {
        reboot();
    } else if (strcmp(cmd, "neofetch") == 0) {
        neofetch();
    } else {
        print_string("Unknown command. Type help.\n");
    }
}
