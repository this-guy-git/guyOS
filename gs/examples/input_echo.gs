// Input demo that stores and reprints user input
#GUYOS_cmd;
#start main;
#include "io.gh";
#include "sys.gh";

title("Input Demo");
clear();
println("Type something and I'll echo it back.");
println("");
input("Say: ");
println("You said:");
println_last();

END;
