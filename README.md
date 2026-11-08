# Hen - Command Line Text Editor
## What is Hen?
Hen is a simple command line text editor, loosely based on existing tools like vim. It can be used for any file format that is text based, even something like a ppm.
## Usage
Just run the executable from the command line, along with a path to the file you want to edit.
```bash
./hen file.txt
```
Navigate the text editor with your arrow keys, as well as page up, page down, home and end. 
## Functions
Certain shortcuts can be used within the editor, these will be updated in later versions. At the moment there is:

- <kbd>Ctrl</kbd>+<kbd>Q</kbd> : Quit the program without saving. You may need to confirm if you haven't saved
- <kbd>Ctrl</kbd>+<kbd>S</kbd> : Save the program. This will prompt for a file name if it is a new file
- <kbd>Ctrl</kbd>+<kbd>F</kbd> : Find a string in the current file.