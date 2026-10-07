# Design Diary – IE3090 RemoteOps

## Project Start

I began the RemoteOps implementation by identifying the required Agent and Controller architecture and calculating my personalised values from registration number IT24100101. The calculated TCP port is 9410 and the Session ID is 1010. The authentication token is OPS-0101.

## Initial Implementation

I implemented the Agent as a TCP server using the BSD sockets API in C. The Controller was implemented as a TCP client. I first established the basic TCP connection and verified communication between the two programs.

## Authentication and System Commands

I then implemented authentication so that a Controller must successfully provide OPS-0101 before using other commands. SYSINFO and LISTPROC were added to provide system information and a process snapshot.

## Restricted EXEC

The EXEC command was implemented using a fixed whitelist consisting of DATE, UPTIME, DISKFREE, HOSTNAME and WHOAMI. Invalid commands such as EXEC LS were rejected instead of allowing arbitrary shell execution.

## File Transfer

PUT and GET were implemented using explicit file-size handling. The uploaded file is stored in the personalised directory ./agentfiles/IT24100101/. File comparison and hash checks were used to verify that downloaded files were byte-for-byte identical to the originals.

## UDP Monitoring

I added a secondary UDP monitoring channel. MONITOR START begins periodic SYSINFO-style UDP messages and MONITOR STOP terminates the stream. The monitoring interval used is 2 seconds.

## Concurrency

The Agent uses pthreads to handle Controller connections. I tested five simultaneous Controller connections to verify that the same Agent can serve multiple sessions at the same time.

## Error Handling and Logging

I tested authentication failures, disallowed EXEC commands, missing files, invalid monitoring operations and graceful QUIT handling. A personalised log file records timestamped connections, commands, monitoring events and file transfers.

## Challenges

One practical issue encountered during development was the TCP port becoming unavailable when more than one Agent process was running. I resolved this by ensuring that only one Agent instance listens on port 9410 before starting Controller tests.

## Final Testing

The completed system was tested across all required functional areas, including TCP communication, authentication, system information, process listing, EXEC, PUT, GET, UDP monitoring, concurrent Controllers, error handling and graceful disconnect.
