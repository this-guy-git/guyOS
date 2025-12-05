// Gets the User's Name and greets them
#GUYOS_cmd; // Marks this file as a guyOS command
#include "io.gh"; // Includes the input/output library
#start main; // Marks the main function as the entry point
// Entry point must return an int (exit code)
// If the exit code is -1, the program requests to exit guyOS shell
// If the exit code is 0, the program ran successfully
// If the exit code is 1, an error occurred, and a message can be attached
// If the exit code is 2, the program requests to restart the program
// If the exit code is blank, the program will stop
// Any other exit code is reserved for future use

int main() {
    str name = name();
    if (name == "\0") {
        io.println("You didn't enter a name.\n");
        return 1, "No name entered"; // Return code of 1 means error occurred, message can be attached, although an exit code of 2 could also work since it would just restart the program
    }
    io.println("Hello, " + name + "!\n");
    return 0;
}

str name() {
    //io.print("Enter your name: "); 
    // uneeded since input() prompts ^^^
    str name = io.input("Enter your name: ");
    return name;
}

END // END to declare EOF