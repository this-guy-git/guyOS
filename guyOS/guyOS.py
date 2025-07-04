import os
import subprocess
from datetime import datetime

class guyOS:
    def __init__(self):
        self.version = "1.0.0"
        # Use real current working directory
        self.current_dir = os.getcwd()
        
    def ls(self):
        """List real directory contents"""
        try:
            items = os.listdir(self.current_dir)
            if not items:
                return "Directory is empty"
            
            output = []
            for item in sorted(items):
                item_path = os.path.join(self.current_dir, item)
                try:
                    stat_info = os.stat(item_path)
                    size = stat_info.st_size
                    modified_time = datetime.fromtimestamp(stat_info.st_mtime).strftime("%Y-%m-%d %H:%M:%S")
                    
                    if os.path.isdir(item_path):
                        output.append(f"{item}/\t\t<DIR>\t\t{modified_time}")
                    else:
                        output.append(f"{item}\t\t{size} bytes\t{modified_time}")
                except (OSError, IOError):
                    output.append(f"{item}\t\t<UNKNOWN>\t<UNKNOWN>")
            
            return "\n".join(output)
        except PermissionError:
            return "Error: Permission denied"
        except FileNotFoundError:
            return "Error: Directory not found"
        except Exception as e:
            return f"Error: {str(e)}"
    
    def cd(self, directory):
        """Change to real directory"""
        if not directory:
            return "Error: No directory specified"
        
        if directory == "..":
            parent_dir = os.path.dirname(self.current_dir)
            if parent_dir != self.current_dir:  # Not at root
                try:
                    os.chdir(parent_dir)
                    self.current_dir = os.getcwd()
                    return f"Changed to directory: {self.current_dir}"
                except PermissionError:
                    return "Error: Permission denied"
                except Exception as e:
                    return f"Error: {str(e)}"
            else:
                return "Already at root directory"
        
        # Handle relative and absolute paths
        if os.path.isabs(directory):
            target_path = directory
        else:
            target_path = os.path.join(self.current_dir, directory)
        
        if os.path.exists(target_path) and os.path.isdir(target_path):
            try:
                os.chdir(target_path)
                self.current_dir = os.getcwd()
                return f"Changed to directory: {self.current_dir}"
            except PermissionError:
                return "Error: Permission denied"
            except Exception as e:
                return f"Error: {str(e)}"
        else:
            return f"Error: Directory '{directory}' not found"
    
    def read(self, *args):
        """Read a real file with optional modifiers"""
        if len(args) < 2:
            return "Usage: read {directory} {filename} [modifiers]\n       read -lines {directory} {filename} {start} {end}\n       read -tail {directory} {filename} {lines}\n       read -head {directory} {filename} {lines}\n       read -count {directory} {filename}"
        
        # Parse modifiers
        modifier = None
        if args[0].startswith('-'):
            modifier = args[0]
            directory = args[1]
            filename = args[2]
            extra_args = args[3:] if len(args) > 3 else []
        else:
            directory = args[0]
            filename = args[1]
            extra_args = []
        
        try:
            # Construct the file path
            if directory:
                if os.path.isabs(directory):
                    file_path = os.path.join(directory, filename)
                else:
                    file_path = os.path.join(self.current_dir, directory, filename)
            else:
                file_path = os.path.join(self.current_dir, filename)
            
            # Normalize the path
            file_path = os.path.normpath(file_path)
            
            if not os.path.exists(file_path):
                return f"Error: File '{filename}' not found"
            
            if not os.path.isfile(file_path):
                return f"Error: '{filename}' is not a file"
            
            # Read the file
            with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
                lines = f.readlines()
            
            # Apply modifiers
            if modifier == "-lines":
                if len(extra_args) >= 2:
                    try:
                        start = int(extra_args[0]) - 1  # Convert to 0-based indexing
                        end = int(extra_args[1])
                        if start < 0:
                            start = 0
                        selected_lines = lines[start:end]
                        content = ''.join(selected_lines)
                        return f"Lines {start+1}-{end} of {filename}:\n{'-' * 30}\n{content}"
                    except ValueError:
                        return "Error: Line numbers must be integers"
                    except IndexError:
                        return "Error: Line range out of bounds"
                else:
                    return "Usage: read -lines {directory} {filename} {start_line} {end_line}"
            
            elif modifier == "-tail":
                if len(extra_args) >= 1:
                    try:
                        num_lines = int(extra_args[0])
                        tail_lines = lines[-num_lines:] if num_lines <= len(lines) else lines
                        content = ''.join(tail_lines)
                        return f"Last {len(tail_lines)} lines of {filename}:\n{'-' * 30}\n{content}"
                    except ValueError:
                        return "Error: Number of lines must be an integer"
                else:
                    return "Usage: read -tail {directory} {filename} {number_of_lines}"
            
            elif modifier == "-head":
                if len(extra_args) >= 1:
                    try:
                        num_lines = int(extra_args[0])
                        head_lines = lines[:num_lines]
                        content = ''.join(head_lines)
                        return f"First {len(head_lines)} lines of {filename}:\n{'-' * 30}\n{content}"
                    except ValueError:
                        return "Error: Number of lines must be an integer"
                else:
                    return "Usage: read -head {directory} {filename} {number_of_lines}"
            
            elif modifier == "-count":
                word_count = sum(len(line.split()) for line in lines)
                char_count = sum(len(line) for line in lines)
                return f"File statistics for {filename}:\nLines: {len(lines)}\nWords: {word_count}\nCharacters: {char_count}"
            
            else:
                # No modifier or unknown modifier - read entire file
                content = ''.join(lines)
                return f"Content of {filename}:\n{'-' * 30}\n{content}"
            
        except PermissionError:
            return f"Error: Permission denied reading '{filename}'"
        except UnicodeDecodeError:
            return f"Error: Cannot read '{filename}' - file may be binary"
        except Exception as e:
            return f"Error reading file: {str(e)}"
    
    def write(self, *args):
        """Write to a real file with optional modifiers"""
        if len(args) < 3:
            return "Usage: write {directory} {filename} {content}\n       write -add {directory} {filename} {content}\n       write -line {directory} {filename} {line_number} {content}\n       write -replace {directory} {filename} {old_text} {new_text}\n       write -insert {directory} {filename} {line_number} {content}"
        
        # Check for modifiers
        modifier = None
        if args[0].startswith('-'):
            modifier = args[0]
            directory = args[1]
            filename = args[2]
            extra_args = args[3:]
        else:
            directory = args[0]
            filename = args[1]
            content = " ".join(args[2:])
            extra_args = []
        
        try:
            # Construct the file path
            if directory:
                if os.path.isabs(directory):
                    dir_path = directory
                    file_path = os.path.join(directory, filename)
                else:
                    dir_path = os.path.join(self.current_dir, directory)
                    file_path = os.path.join(self.current_dir, directory, filename)
            else:
                dir_path = self.current_dir
                file_path = os.path.join(self.current_dir, filename)
            
            # Normalize the paths
            dir_path = os.path.normpath(dir_path)
            file_path = os.path.normpath(file_path)
            
            # Create directory if it doesn't exist
            if not os.path.exists(dir_path):
                os.makedirs(dir_path)
            
            # Handle different modifiers
            if modifier == "-add":
                content = " ".join(extra_args)
                # Check if file exists to determine if we need a newline
                file_exists = os.path.exists(file_path)
                with open(file_path, 'a', encoding='utf-8') as f:
                    if file_exists:
                        f.write('\n' + content)
                    else:
                        f.write(content)
                return f"Content added to '{filename}' at {file_path}"
            
            elif modifier == "-line":
                if len(extra_args) >= 2:
                    try:
                        line_number = int(extra_args[0]) - 1  # Convert to 0-based indexing
                        new_content = " ".join(extra_args[1:])
                        
                        # Read existing file if it exists
                        lines = []
                        if os.path.exists(file_path):
                            with open(file_path, 'r', encoding='utf-8') as f:
                                lines = f.readlines()
                        
                        # Extend lines list if necessary
                        while len(lines) <= line_number:
                            lines.append('\n')
                        
                        # Replace the specific line
                        lines[line_number] = new_content + '\n'
                        
                        # Write back to file
                        with open(file_path, 'w', encoding='utf-8') as f:
                            f.writelines(lines)
                        
                        return f"Line {line_number + 1} in '{filename}' replaced successfully"
                    except ValueError:
                        return "Error: Line number must be an integer"
                else:
                    return "Usage: write -line {directory} {filename} {line_number} {content}"
            
            elif modifier == "-replace":
                if len(extra_args) >= 2:
                    old_text = extra_args[0]
                    new_text = " ".join(extra_args[1:])
                    
                    if not os.path.exists(file_path):
                        return f"Error: File '{filename}' does not exist"
                    
                    # Read file content
                    with open(file_path, 'r', encoding='utf-8') as f:
                        content = f.read()
                    
                    # Replace text
                    if old_text in content:
                        new_content = content.replace(old_text, new_text)
                        with open(file_path, 'w', encoding='utf-8') as f:
                            f.write(new_content)
                        return f"Text replaced in '{filename}' successfully"
                    else:
                        return f"Text '{old_text}' not found in '{filename}'"
                else:
                    return "Usage: write -replace {directory} {filename} {old_text} {new_text}"
            
            elif modifier == "-insert":
                if len(extra_args) >= 2:
                    try:
                        line_number = int(extra_args[0]) - 1  # Convert to 0-based indexing
                        new_content = " ".join(extra_args[1:])
                        
                        # Read existing file if it exists
                        lines = []
                        if os.path.exists(file_path):
                            with open(file_path, 'r', encoding='utf-8') as f:
                                lines = f.readlines()
                        
                        # Insert new line at specified position
                        if line_number <= len(lines):
                            lines.insert(line_number, new_content + '\n')
                        else:
                            # If line number is beyond file, pad with empty lines
                            while len(lines) < line_number:
                                lines.append('\n')
                            lines.append(new_content + '\n')
                        
                        # Write back to file
                        with open(file_path, 'w', encoding='utf-8') as f:
                            f.writelines(lines)
                        
                        return f"Content inserted at line {line_number + 1} in '{filename}' successfully"
                    except ValueError:
                        return "Error: Line number must be an integer"
                else:
                    return "Usage: write -insert {directory} {filename} {line_number} {content}"
            
            else:
                # No modifier - write entire file (overwrite)
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(content)
                return f"File '{filename}' written successfully to {file_path}"
            
        except PermissionError:
            return f"Error: Permission denied writing to '{filename}'"
        except Exception as e:
            return f"Error writing file: {str(e)}"
    
    def ver(self):
        """Show version information"""
        return f"guyOS Version {self.version}\nCopyright (c) 2024 Guy Industries\nMock Operating System"
    
    def help(self):
        """Show help information"""
        help_text = """
guyOS Commands:
===============

ls                           - List REAL directory contents
cd {directory}              - Change to REAL directory (use '..' for parent directory)
read {directory} {filename} - Read a REAL file (directory can be empty for current dir)
read -lines {directory} {filename} {start} {end} - Read specific lines from file
read -tail {directory} {filename} {lines} - Read last N lines from file
read -head {directory} {filename} {lines} - Read first N lines from file
read -count {directory} {filename} - Show file statistics (lines, words, characters)
write {directory} {filename} {content} - Write to a REAL file (overwrites existing)
write -add {directory} {filename} {content} - Add content to a REAL file on new line
write -line {directory} {filename} {line_number} {content} - Replace specific line
write -replace {directory} {filename} {old_text} {new_text} - Replace text in file
write -insert {directory} {filename} {line_number} {content} - Insert line at position
guython {script_file.gy/.guy} [args] - Execute Guython script file (.gy or .guy)
guython -c "code"           - Execute Guython code directly
ver                         - Show version information
help                        - Show this help message
pwd                         - Show current REAL directory
exit                        - Exit guyOS
        """
        return help_text.strip()
    
    def guython(self, *args):
        """Execute Guython scripts or commands"""
        import getpass
        
        # Get current username
        username = getpass.getuser()
        guython_path = f"C:\\Users\\{username}\\AppData\\Local\\Programs\\Guython\\guython.exe"
        
        # Check if Guython interpreter exists
        if not os.path.exists(guython_path):
            return f"Error: Guython interpreter not found at {guython_path}\nPlease install Guython or check the installation path."
        
        if len(args) == 0:
            return "Usage: guython {script_file}"
        
        try:
            # Handle different argument patterns
            if args[0] == "-c":
                # Execute code directly
                if len(args) < 2:
                    return "Error: No code provided for -c option"
                code = " ".join(args[1:])
                result = subprocess.run([guython_path, "-c", code], 
                                      capture_output=True, text=True, cwd=self.current_dir)
            
            else:
                # Execute script file
                script_file = args[0]
                script_args = list(args[1:]) if len(args) > 1 else []
                
                # Check if script file exists (handle relative/absolute paths)
                if not os.path.isabs(script_file):
                    script_path = os.path.join(self.current_dir, script_file)
                else:
                    script_path = script_file
                
                if not os.path.exists(script_path):
                    return f"Error: Script file '{script_file}' not found"
                
                # Execute the script
                cmd = [guython_path, script_path] + script_args
                result = subprocess.run(cmd, capture_output=True, text=True, cwd=self.current_dir)
            
            # Format output
            output = []
            if result.stdout:
                output.append("Output:")
                output.append("-" * 20)
                output.append(result.stdout.strip())
            
            if result.stderr:
                if output:
                    output.append("")
                output.append("Errors:")
                output.append("-" * 20)
                output.append(result.stderr.strip())
            
            if result.returncode != 0:
                if output:
                    output.append("")
                output.append(f"Process exited with code: {result.returncode}")
            
            return "\n".join(output) if output else "Script executed successfully (no output)"
            
        except subprocess.TimeoutExpired:
            return "Error: Guython execution timed out"
        except subprocess.CalledProcessError as e:
            return f"Error: Guython execution failed with code {e.returncode}\n{e.stderr}"
        except FileNotFoundError:
            return f"Error: Could not execute Guython interpreter at {guython_path}"
    def pwd(self):
        """Print working directory"""
        return f"Current real directory: {self.current_dir}"
    
    def run(self):
        """Main loop for the operating system"""
        print("=" * 50)
        print("Welcome to guyOS!")
        print("=" * 50)
        print("Type 'help' for available commands or 'exit' to quit.")
        print("All commands now work with your computer's real file system!")
        print()
        
        while True:
            try:
                # Show prompt
                prompt = f"guyOS:{self.current_dir}$ "
                user_input = input(prompt).strip()
                
                if not user_input:
                    continue
                
                # Parse command
                parts = user_input.split()
                command = parts[0].lower()
                args = parts[1:]
                
                # Execute commands
                if command == "exit":
                    print("Goodbye from guyOS!")
                    break
                elif command == "ls":
                    print(self.ls())
                elif command == "cd":
                    directory = args[0] if args else ""
                    print(self.cd(directory))
                elif command == "read":
                    if len(args) >= 2:
                        # Pass all arguments to read method to handle modifiers
                        print(self.read(*args))
                    else:
                        print("Usage: read {directory} {filename} [modifiers]")
                        print("       read -lines {directory} {filename} {start} {end}")
                        print("       read -tail {directory} {filename} {lines}")
                        print("       read -head {directory} {filename} {lines}")
                        print("       read -count {directory} {filename}")
                elif command == "write":
                    if len(args) >= 3:
                        # Pass all arguments to write method to handle -add modifier
                        print(self.write(*args))
                    else:
                        print("Usage: write {directory} {filename} {content}")
                        print("       write -add {directory} {filename} {content}")
                        print("       write -line {directory} {filename} {line_number} {content}")
                        print("       write -replace {directory} {filename} {old_text} {new_text}")
                        print("       write -insert {directory} {filename} {line_number} {content}")
                elif command == "guython":
                    if len(args) >= 1:
                        print(self.guython(*args))
                    else:
                        print("Usage: guython {script_file.gy/.guy} [args]")
                        print("       guython -c \"code to execute\"")
                elif command == "ver":
                    print(self.ver())
                elif command == "help":
                    print(self.help())
                elif command == "pwd":
                    print(self.pwd())
                else:
                    print(f"Unknown command: {command}")
                    print("Type 'help' for available commands")
                
                print()  # Add blank line for readability
                
            except KeyboardInterrupt:
                print("\nUse 'exit' to quit guyOS")
            except EOFError:
                print("\nGoodbye")
                break

# Run GuyOS
if __name__ == "__main__":
    os_instance = guyOS()
    os_instance.run()