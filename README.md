# networked-spell-checker
# INTRODUCTION:
This program is a networked spelled checker. The program is launched with a dictionary file used to check the correctness of a word sent from a client, and a port number where the client will connect from. Once a client is connected, they can enter one number to be handled by a worker thread, if one is available. The worker thread will then check the correctness of the word, and send the result back to the client and put the result into a log buffer. A logger thread will then write the contents of the log buffer into a log file to keep history of all words and results

# DESIGN:
* The program begins by launching with arguments for the dictionary file and port number. If no arugment is specfied, the default dictionary file is "dictionary.txt", and the default port number is 8888. The server can be launched by entering ./net arg1 arg2. Setting the dictionary file name and the port number for the server is handled by the launch() function.

* The dictionary file is then loaded into a dictionary buffer. The buffer is an array of strings and the buffer will be used to check the correct of the word sent by the client. The loading of the dictionary to the buffer is handled by load_file_buffer().

* The condition variables to be used for the client buffer and the log buffer with pthread_cond_init(). Both the client and log buffer have an empty condition to signal that there is room on the buffer, and a full condition to signal that the buffer is full and nothing else can be added to the buffer unless something is removed.

* The logger thread is created and immediately set to wait until there is something on the log buffer to be written to the log file. logger_thread() handles writing the word and correctness to a log file while the log buffer is not empty, and waits until the buffer has something in it.

* The pool of worker threads is then initialized and immediately set to wait until there is a client to handle in the client buffer. client_threads() handles takes a client out of the client buffer, that is protected by a mutex. The thread will wait for the client to enter a word indefinitely until the client disconnects itself. Once the client enters a word, the word is checked against the dictionary buffer. If the word is in the dictionary buffer, the correctness of the word is set to "Correct!", and "Incorrect Spelling" if the word is not in the dictionary. The result is a combination of the word and correctness, and the result is put onto the log buffer by the client thread to be processed by the log thread.

* We then allow connections to the server by calling connect(). This function sets up the server with the port number that will be opened for connection for clients. Once a client connects, the client's socket id is added to the client buffer to be handled by a worker thread. The server will stay open until the program terminated.

# TESTING:
* First, I tested that the server could take conenctions by launching it and ensuring that the bind is successful and that the server is waiting for connections with the default dictionary (dictionary.txt) and the default port (8888).

* Next, I launched the server with different parameters to make sure it properly launched. I used 9999 as the port number and test.txt as the dictionary file. I launched the server with<br>
	
	./net 9999<br>
	./net test.txt<br>
	./net 9999 test.txt<br>
	./net test.txt 9999<br>
	
to determine if the server could launch with a parameter. If the parameters are accepted, they are printed to the screen e.g.<br>
	
	Dictionary: test.txt
	Port Number: 9999
	
* To test that the server can take a new connection, I opened a new terminal window and entered "telnet 127.0.0.1 8888" into the command line. If the client connects properly, the server will write to the client "The server has received your connection. You will be handled shortly." 8888 can be whatever port is open on the server.

* To test that the server can handle multiple client connections, I repeated the step above with multiple terminal windows to ensure that the server can indeed handle multiple clients

* Once a client is connected, I started by sending words to the server and having the server send the word back to the server to ensure that the server is receiving the word. Once I got that to work, I then had the server perform the spell check and then print the word received and the correctness of the word. The server then wrote the correctness to the client.  Once the spell checking was correct, I concatenated the word and the correctness into a result string and wrote that string to the log file. To ensure that the write was performed, I simply opened the log file.
