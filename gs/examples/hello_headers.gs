// Minimal hello that exercises includes and multiple prints
#GUYOS_cmd;
#start main;
#include "io.gh";
#include "sys.gh";

println("Hello from GScript with headers!");
print("Kernel: "); println(kernel_version()); // ignored by compiler but kept for future
println("Goodbye!\n");

END;
