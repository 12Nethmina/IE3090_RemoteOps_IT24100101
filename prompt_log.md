# AI Prompt Log – IE3090 RemoteOps

## AI Tool Used

ChatGPT

## Interaction 1 – Project Setup and Personalisation

### Prompt
Asked for guidance to start the IE3090 RemoteOps assignment and calculate the personalised values from registration number IT24100101.

### Use of Output
The output was used to determine the required TCP port, SID, authentication token, source file names, log file name, storage path and ZIP name.

### Verification/Changes
I checked the calculated values against the assignment brief and used my own registration number.

---

## Interaction 2 – TCP Agent and Controller

### Prompt
Asked for guidance and code structure for implementing the RemoteOps Agent and Controller using C and BSD sockets.

### Use of Output
The suggested socket structure and communication approach were used as a starting point for the TCP implementation.

### Verification/Changes
I compiled the programs with gcc and tested the TCP connection locally. I modified the code during implementation and checked the actual program output.

---

## Interaction 3 – Authentication and Command Handling

### Prompt
Asked for implementation guidance for AUTH, SYSINFO, LISTPROC and the restricted EXEC command.

### Use of Output
The suggestions were used to implement the command handlers and authentication flow.

### Verification/Changes
I tested valid authentication, invalid authentication and invalid EXEC commands. I rejected commands outside the required EXEC whitelist.

---

## Interaction 4 – File Transfer

### Prompt
Asked for guidance implementing PUT and GET with exact file-size handling.

### Use of Output
The guidance was used to implement byte-counted TCP file transfers.

### Verification/Changes
I tested file uploads and downloads and compared the original and downloaded files to verify byte-for-byte integrity.

---

## Interaction 5 – UDP Monitoring

### Prompt
Asked for the full Step 10 implementation for MONITOR START, UDP monitoring and MONITOR STOP.

### Use of Output
The code was used as a basis for adding UDP monitoring to the Agent and Controller.

### Verification/Changes
I compiled and ran the programs, verified periodic UDP SYSINFO messages, tested MONITOR STOP and confirmed that the packets included SID:1010.

---

## Interaction 6 – Testing and Error Handling

### Prompt
Asked for clear step-by-step testing procedures for errors, graceful disconnect and multiple simultaneous Controllers.

### Use of Output
The procedures were used to test the completed system.

### Verification/Changes
I tested unauthenticated commands, invalid EXEC commands, missing files, MONITOR STOP without START, unknown commands and QUIT. I also tested five simultaneous Controller connections.

---

## Reflection on AI Use

AI was used as a development and learning aid rather than as a replacement for testing. I compiled the code, ran the programs, checked the outputs and made changes when required. I also used the explanations to understand the socket communication, concurrency, authentication, file transfer and UDP monitoring mechanisms.
