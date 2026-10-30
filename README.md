Simple and lightweight tool to send files on local networks through tcp connection
---
Examples of usage:

Send file:
```bash
  file-drop path-to-file
```
Recieve file:
```bash
  file-drop sender-ip
```
It will automatically set the name of file to the name that sender has on this file.
Or if you want to specify file name:
```bash
  file-drop sender-ip file-name
```