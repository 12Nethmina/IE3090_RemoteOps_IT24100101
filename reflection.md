# Reflection – IE3090 RemoteOps

## 1. Which AI tools did you use, and at which stages?

I used ChatGPT during the Part 1 development of the RemoteOps system. I used it mainly for understanding the assignment requirements, planning the implementation steps, generating and explaining C socket programming structures, debugging implementation issues, and preparing testing procedures. AI assistance was used during TCP communication, authentication, system information, process listing, EXEC handling, file transfers, UDP monitoring, error handling and final testing.

## 2. What did the AI do well? Where did it get things wrong or mislead you?

The AI was useful for breaking the assignment into smaller implementation and testing stages. It helped explain concepts such as TCP client-server communication, pthread-based concurrency, exact file-size handling and UDP monitoring. It also helped identify the required personalised values and provided examples of expected command responses.

However, some generated code and suggestions required careful checking against my existing implementation. Some code could not be inserted directly without adapting it to the variables and structure already present in my project. This showed me that generated code is not automatically correct for an existing codebase and must be compiled and tested before use.

## 3. What did you change, add, or reject from AI output, and why?

I modified generated code to match my own personalised values, existing source-code structure and assignment protocol. I checked that the TCP port, SID, authentication token, filenames, log file and storage path were correct. I also tested the generated logic using gcc and actual Agent and Controller executions. Invalid commands were tested to make sure the EXEC whitelist was enforced. File transfers were tested using file comparison and hash verification. I did not rely only on the expected output provided by AI; I used the actual program output as the basis for verification.

## 4. What did you learn about your own understanding of network programming?

This assignment improved my understanding of practical socket programming. I learned how a TCP server creates a listening socket, accepts connections and handles individual clients concurrently using pthreads. I also learned why TCP applications must deal with message framing and exact byte counts instead of assuming that one send() matches one recv(). The file-transfer implementation helped me understand reliable byte-based communication. UDP monitoring showed me the difference between a reliable TCP control channel and a lightweight UDP monitoring channel. Testing multiple clients and error conditions also improved my understanding of concurrent sessions, validation and graceful disconnection.

