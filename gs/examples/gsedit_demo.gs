// gsedit_demo: line-oriented editor demo using slots and file load/save
#GUYOS_cmd;
#start main;
#include "io.gh";
#include "sys.gh";

title("gsedit demo");
clear();
println("gsedit demo (up to 5 lines)");
println("Loading existing file if present...");
load_file("notes.txt");
println("");
println("Edit lines (leave blank to keep existing):");

input("1> "); store(0);
input("2> "); store(1);
input("3> "); store(2);
input("4> "); store(3);
input("5> "); store(4);

println("");
println("Saving to notes.txt...");
save_file("notes.txt", 5);
println("Saved. Current contents:");
load_println(0);
load_println(1);
load_println(2);
load_println(3);
load_println(4);
println("");
println("Done. Re-run to edit again.");

END;
