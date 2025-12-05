// Banner demo: clears screen, sets title, prints info
#GUYOS_cmd;
#start main;
#include "io.gh";
#include "sys.gh";

clear();
title("GScript Banner");
println("======================");
println("   Welcome to GScript   ");
println("======================");
println("");
println("This app uses the new gxe VM ops:");
println("- clear() to wipe the body");
println("- title(\"...\") to set the top bar");
println("");
println("Have fun!");

END;
