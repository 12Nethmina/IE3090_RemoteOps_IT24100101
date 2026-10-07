# IE3090 RemoteOps

## Student Information

- Registration Number: IT24100101
- Student Name: Karunarathna S.C.N

## Project

RemoteOps is a remote system monitoring and management tool implemented in C using BSD sockets.

The system consists of:

- **Agent** – server/managed machine
- **Controller** – client/administrator

## Personalisation

| Item | Value |
|---|---|
| Registration Number | IT24100101 |
| TCP Port | 9410 |
| Agent | agent_101.c |
| Controller | controller_101.c |
| Makefile | Makefile_101 |
| SID | 1010 |
| Authentication Token | OPS-0101 |
| Log File | remoteops_IT24100101.log |
| Storage Path | ./agentfiles/IT24100101/ |
| UDP Monitoring Port | 9411 |
| UDP Monitoring Interval | 2 seconds |
| ZIP Archive | IE3090_IT24100101.zip |

## Implemented Features

- Multiple simultaneous Controller connections using pthreads
- Authentication
- SYSINFO
- LISTPROC
- Restricted EXEC commands
- PUT file upload
- GET file download
- UDP system monitoring
- Graceful disconnect
- Timestamped logging

## EXEC Whitelist

The following commands are supported:

- DATE
- UPTIME
- DISKFREE
- HOSTNAME
- WHOAMI

Arbitrary shell commands are rejected.

## Build

Compile the Agent:

```bash
gcc -Wall -Wextra -pedantic agent_101.c -o agent_101 -pthread
